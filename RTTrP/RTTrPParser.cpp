#include "RTTrPParser.hpp"

#include <bit>
#include <cstring>

namespace RTTrP
{
namespace
{
// Byte-order aware reads. `big_endian` is the byte order of the packet, as
// announced by its signature; the host is assumed little or big endian and
// detected at compile time.
constexpr bool host_big_endian = std::endian::native == std::endian::big;

template <typename T>
T read_value(const uint8_t*& ptr, bool big_endian) noexcept
{
  T value;
  std::memcpy(&value, ptr, sizeof(T));
  ptr += sizeof(T);

  if(sizeof(T) > 1 && big_endian != host_big_endian)
  {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);
    for(std::size_t i = 0; i < sizeof(T) / 2; ++i)
      std::swap(bytes[i], bytes[sizeof(T) - 1 - i]);
  }
  return value;
}

uint8_t read_u8(const uint8_t*& ptr) noexcept
{
  return *ptr++;
}

// Everything a sub-module needs to be decoded: the payload after the 3-byte
// module header and how many bytes of it there are.
struct Payload
{
  const uint8_t* ptr;
  const uint8_t* end;
  bool big_endian;

  std::size_t remaining() const noexcept { return std::size_t(end - ptr); }
  bool has(std::size_t n) const noexcept { return remaining() >= n; }

  template <typename T>
  T read() noexcept
  {
    return read_value<T>(ptr, big_endian);
  }
};

void parse_centroid(Payload& p, ParsedTrackable& out)
{
  // latency u16, x y z f64
  if(!p.has(2 + 3 * 8))
    return;
  out.latency = p.read<uint16_t>();
  out.x = p.read<double>();
  out.y = p.read<double>();
  out.z = p.read<double>();
  out.has_centroid = true;
}

void parse_quaternion(Payload& p, ParsedTrackable& out)
{
  // latency u16, qx qy qz qw f64
  if(!p.has(2 + 4 * 8))
    return;
  out.latency = p.read<uint16_t>();
  out.qx = p.read<double>();
  out.qy = p.read<double>();
  out.qz = p.read<double>();
  out.qw = p.read<double>();
  out.has_quaternion = true;
}

void parse_euler(Payload& p, ParsedTrackable& out)
{
  // latency u16, order u16, r1 r2 r3 f64
  if(!p.has(2 + 2 + 3 * 8))
    return;
  out.latency = p.read<uint16_t>();
  out.euler_order = p.read<uint16_t>();
  out.roll = p.read<double>();
  out.pitch = p.read<double>();
  out.yaw = p.read<double>();
  out.has_euler = true;
}

void parse_led(Payload& p, ParsedTrackable& out)
{
  // latency u16, x y z f64, index u8
  if(!p.has(2 + 3 * 8 + 1))
    return;
  ParsedTrackable::LEDData led;
  led.latency = p.read<uint16_t>();
  led.x = p.read<double>();
  led.y = p.read<double>();
  led.z = p.read<double>();
  led.index = read_u8(p.ptr);
  out.leds.push_back(led);
}

void parse_centroid_accvel(Payload& p, ParsedTrackable& out)
{
  // x y z f64, ax ay az f32, vx vy vz f32
  if(!p.has(3 * 8 + 6 * 4))
    return;
  out.x = p.read<double>();
  out.y = p.read<double>();
  out.z = p.read<double>();
  out.ax = p.read<float>();
  out.ay = p.read<float>();
  out.az = p.read<float>();
  out.vx = p.read<float>();
  out.vy = p.read<float>();
  out.vz = p.read<float>();
  out.has_centroid = true;
  out.has_velocity = true;
}

void parse_led_accvel(Payload& p, ParsedTrackable& out)
{
  // x y z f64, ax ay az f32, vx vy vz f32, index u8
  if(!p.has(3 * 8 + 6 * 4 + 1))
    return;
  ParsedTrackable::LEDData led;
  led.x = p.read<double>();
  led.y = p.read<double>();
  led.z = p.read<double>();
  led.ax = p.read<float>();
  led.ay = p.read<float>();
  led.az = p.read<float>();
  led.vx = p.read<float>();
  led.vy = p.read<float>();
  led.vz = p.read<float>();
  led.index = read_u8(p.ptr);
  led.has_velocity = true;
  out.leds.push_back(led);
}

void parse_zones(Payload& p, ParsedTrackable& out)
{
  // number of zones u8, then per zone: size u8 (incl. itself), name length u8, name
  if(!p.has(1))
    return;
  const uint8_t num_zones = read_u8(p.ptr);
  for(int z = 0; z < num_zones && p.has(2); ++z)
  {
    const uint8_t* zone_start = p.ptr;
    const uint8_t zone_size = read_u8(p.ptr);
    const uint8_t name_len = read_u8(p.ptr);
    if(!p.has(name_len))
      break;
    out.zones.emplace_back(reinterpret_cast<const char*>(p.ptr), name_len);

    // Step by the declared size when it is consistent, else by what we read.
    if(zone_size >= 2 + name_len && zone_start + zone_size <= p.end)
      p.ptr = zone_start + zone_size;
    else
      p.ptr += name_len;
  }
}
}

bool parse_header(const uint8_t* data, std::size_t size, PacketHeader& out)
{
  if(!data || size < RTTRP_HEADER_SIZE)
    return false;

  const uint8_t* ptr = data;

  // The signature tells the byte order of everything that follows
  uint16_t int_sig;
  std::memcpy(&int_sig, ptr, sizeof(int_sig));
  // Read natively, "AT" (0x4154) means the packet shares the host's byte order and
  // "TA" (0x5441) that it has the other one: on a little endian host, the
  // bytes 41 54 of a big endian packet read as 0x5441.
  if(int_sig == RTTRP_INT_HEADER_BIG)
    out.big_endian = host_big_endian;
  else if(int_sig == RTTRP_INT_HEADER_LITTLE)
    out.big_endian = !host_big_endian;
  else
    return false;

  out.int_signature = read_value<uint16_t>(ptr, out.big_endian);
  out.float_signature = read_value<uint16_t>(ptr, out.big_endian);
  out.version = read_value<uint16_t>(ptr, out.big_endian);
  out.packet_id = read_value<uint32_t>(ptr, out.big_endian);
  out.format = read_u8(ptr);
  out.size = read_value<uint16_t>(ptr, out.big_endian);
  out.context = read_value<uint32_t>(ptr, out.big_endian);
  out.num_modules = read_u8(ptr);
  return true;
}

bool parse_trackable(
    const uint8_t* module, std::size_t module_size, bool big_endian,
    ParsedTrackable& out)
{
  // type u8, size u16, name length u8, name, [timestamp u32], sub-module count u8
  if(!module || module_size < RTTRP_MODULE_HEADER_SIZE + 2)
    return false;

  const uint8_t type = module[0];
  const bool with_timestamp = type == uint8_t(ModuleType::TrackableWithTimestamp);
  if(!with_timestamp && type != uint8_t(ModuleType::Trackable))
    return false;

  Payload p{module + RTTRP_MODULE_HEADER_SIZE, module + module_size, big_endian};

  const uint8_t name_len = read_u8(p.ptr);
  if(!p.has(name_len))
    return false;
  out.name.assign(reinterpret_cast<const char*>(p.ptr), name_len);
  p.ptr += name_len;

  if(with_timestamp)
  {
    if(!p.has(4))
      return false;
    out.timestamp = p.read<uint32_t>();
    out.has_timestamp = true;
  }

  if(!p.has(1))
    return false;
  const uint8_t num_sub_modules = read_u8(p.ptr);

  for(int i = 0; i < num_sub_modules && p.has(RTTRP_MODULE_HEADER_SIZE); ++i)
  {
    const uint8_t* sub_start = p.ptr;
    const uint8_t sub_type = read_u8(p.ptr);
    const uint16_t sub_size = p.read<uint16_t>();

    // A size smaller than its own header is corrupt; a size past the end is
    // truncated: in both cases there is nothing more to read from this module.
    if(sub_size < RTTRP_MODULE_HEADER_SIZE || sub_start + sub_size > p.end)
      break;

    Payload sub{p.ptr, sub_start + sub_size, big_endian};
    switch(static_cast<ModuleType>(sub_type))
    {
      case ModuleType::CentroidMod:
        parse_centroid(sub, out);
        break;
      case ModuleType::QuatModule:
        parse_quaternion(sub, out);
        break;
      case ModuleType::EulerModule:
        parse_euler(sub, out);
        break;
      case ModuleType::LEDModule:
        parse_led(sub, out);
        break;
      case ModuleType::CentroidAccVelMod:
        parse_centroid_accvel(sub, out);
        break;
      case ModuleType::LEDAccVelMod:
        parse_led_accvel(sub, out);
        break;
      case ModuleType::ZoneMod:
        parse_zones(sub, out);
        break;
      default:
        // Unknown sub-module: skipped thanks to its size
        break;
    }

    p.ptr = sub_start + sub_size;
  }

  return true;
}

bool parse_packet(const uint8_t* data, std::size_t size, ParsedPacket& out)
{
  out.trackables.clear();
  out.unknown_modules = 0;

  if(!parse_header(data, size, out.header))
    return false;

  const uint8_t* ptr = data + RTTRP_HEADER_SIZE;
  const uint8_t* end = data + size;
  const bool big_endian = out.header.big_endian;

  for(int i = 0; i < out.header.num_modules && ptr + RTTRP_MODULE_HEADER_SIZE <= end;
      ++i)
  {
    const uint8_t* module = ptr;
    const uint8_t type = module[0];
    const uint8_t* size_ptr = module + 1;
    const uint16_t mod_size = read_value<uint16_t>(size_ptr, big_endian);

    if(mod_size < RTTRP_MODULE_HEADER_SIZE || module + mod_size > end)
      break;

    if(type == uint8_t(ModuleType::Trackable)
       || type == uint8_t(ModuleType::TrackableWithTimestamp))
    {
      ParsedTrackable trackable;
      if(parse_trackable(module, mod_size, big_endian, trackable))
        out.trackables.push_back(std::move(trackable));
    }
    else
    {
      out.unknown_modules++;
    }

    ptr = module + mod_size;
  }

  return true;
}
}
