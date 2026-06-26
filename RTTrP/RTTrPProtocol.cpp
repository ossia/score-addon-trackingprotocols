#include "RTTrPProtocol.hpp"
#include "../Common/TrackingTreeBuilder.hpp"

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/name_validation.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>

#include <cstring>

namespace RTTrP
{

// RTTrP header constants
constexpr uint16_t RTTRP_INT_HEADER_BIG = 0x4154;    // 'AT' in big endian
constexpr uint16_t RTTRP_INT_HEADER_LITTLE = 0x5441; // 'TA' in little endian

template<typename T>
T RTTrPProtocol::read_value(const uint8_t*& ptr, bool swap_bytes)
{
  T value;
  std::memcpy(&value, ptr, sizeof(T));
  ptr += sizeof(T);

  if (swap_bytes && sizeof(T) > 1)
  {
    // Swap bytes for multi-byte types
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);
    for (size_t i = 0; i < sizeof(T) / 2; ++i)
    {
      std::swap(bytes[i], bytes[sizeof(T) - 1 - i]);
    }
  }

  return value;
}

RTTrPProtocol::RTTrPProtocol(
    const ossia::net::network_context_ptr& ctx,
    const RTTrPSpecificSettings& settings)
    : ossia::net::protocol_base{}
    , m_ctx{ctx}
    , m_settings{settings}
    , m_trackable_slots{settings.numTrackables, Tracking::SlotStrategy::NameBased}
{
}

RTTrPProtocol::~RTTrPProtocol()
{
  stop_receive();
}

void RTTrPProtocol::set_device(ossia::net::device_base& dev)
{
  m_device = &dev;

  auto& root = dev.get_root_node();
  create_device_tree(root);

  setup_receive_socket();
}

void RTTrPProtocol::create_device_tree(ossia::net::node_base& root)
{
  using TB = Tracking::TreeBuilder;

  // Frame-level parameters
  TB::create_int_param(root, "frame");
  TB::create_int_param(root, "timestamp");

  // Create trackable slots
  auto* trackables_node = root.create_child("trackables");
  for (int i = 0; i < m_settings.numTrackables; ++i)
  {
    auto* slot_node = trackables_node->create_child(std::to_string(i));

    TB::create_bool_param(*slot_node, "active", false);
    TB::create_string_param(*slot_node, "name", "");
    TB::create_vec3f_param(*slot_node, "position");

    if (m_settings.enableQuaternion)
    {
      TB::create_vec4f_param(*slot_node, "quaternion", {0.f, 0.f, 0.f, 1.f});
    }

    if (m_settings.enableEuler)
    {
      TB::create_vec3f_param(*slot_node, "euler", {0.f, 0.f, 0.f},
                             static_cast<float>(-M_PI), static_cast<float>(M_PI));
    }

    if (m_settings.enableVelocity)
    {
      TB::create_vec3f_param(*slot_node, "velocity");
    }

    if (m_settings.enableAcceleration)
    {
      TB::create_vec3f_param(*slot_node, "acceleration");
    }

    TB::create_int_param(*slot_node, "latency");

    // Create LED slots
    if (m_settings.maxLEDsPerTrackable > 0)
    {
      TB::create_led_slots(*slot_node, m_settings.maxLEDsPerTrackable);
    }
  }

  // Create zones container (populated dynamically)
  if (m_settings.enableZones)
  {
    root.create_child("zones");
  }
}

void RTTrPProtocol::setup_receive_socket()
{
  try
  {
    ossia::net::inbound_socket_configuration config{
        .bind = "0.0.0.0",
        .port = m_settings.port
    };

    m_receive_socket = std::make_unique<ossia::net::udp_receive_socket>(
        config, m_ctx->context);

    m_receive_socket->open();
    m_receive_socket->receive(
        [this](const char* data, std::size_t sz)
        {
          this->on_received_data(data, sz);
        });
  }
  catch (const std::exception& e)
  {
    ossia::logger().error("RTTrP: Failed to setup receive socket: {}", e.what());
  }
}

void RTTrPProtocol::stop_receive()
{
  if (m_receive_socket)
  {
    m_receive_socket->close();
    m_receive_socket.reset();
  }
}

void RTTrPProtocol::on_received_data(const char* data, std::size_t size)
{
  if (!m_device || size < 4)
    return;

  parse_packet(reinterpret_cast<const uint8_t*>(data), size);
}

bool RTTrPProtocol::parse_packet(const uint8_t* data, std::size_t size)
{
  if (size < 12)
    return false;

  const uint8_t* ptr = data;
  const uint8_t* end = data + size;

  // Read header signature
  uint16_t int_header;
  std::memcpy(&int_header, ptr, sizeof(int_header));
  ptr += 2;

  // Determine endianness
  if (int_header == RTTRP_INT_HEADER_BIG)
  {
    m_little_endian = false;
  }
  else if (int_header == RTTRP_INT_HEADER_LITTLE)
  {
    m_little_endian = true;
  }
  else
  {
    return false; // Invalid header
  }

  // Skip float header (2 bytes), version (2 bytes)
  ptr += 4;

  // Read packet ID
  m_packet_id = read_value<uint32_t>(ptr, !m_little_endian);

  // Drop duplicate packets (retransmit / loopback echo).
  if (m_have_prev_packet_id && m_packet_id == m_prev_packet_id)
    return false;
  m_prev_packet_id = m_packet_id;
  m_have_prev_packet_id = true;

  // Read packet format (1 byte)
  uint8_t pkt_format = *ptr++;

  // Read packet size (2 bytes)
  uint16_t pkt_size = read_value<uint16_t>(ptr, !m_little_endian);

  // Read context (4 bytes)
  uint32_t context = read_value<uint32_t>(ptr, !m_little_endian);

  // Read number of modules (1 byte)
  uint8_t num_modules = *ptr++;

  // Update frame info
  auto& root = m_device->get_root_node();
  Tracking::TreeBuilder::update_param(&root, "frame", (int)m_packet_id);

  // Parse each module
  for (int i = 0; i < num_modules && ptr < end; ++i)
  {
    if (ptr + 3 > end)
      break;

    // Module header: type (1 byte), size (2 bytes)
    uint8_t mod_type = *ptr++;
    uint16_t mod_size = read_value<uint16_t>(ptr, !m_little_endian);

    if (ptr + mod_size > end)
      break;

    if (mod_type == static_cast<uint8_t>(ModuleType::Trackable))
    {
      ParsedTrackable trackable;
      if (parse_trackable(ptr, mod_size, trackable))
      {
        int slot = m_trackable_slots.find_or_allocate(trackable.name);
        if (slot >= 0 && slot < m_settings.numTrackables)
        {
          m_trackable_slots.slot_info(slot).active = true;
          update_trackable_parameters(slot, trackable);

          // Update zones
          for (const auto& zone : trackable.zones)
          {
            update_zone_parameter(zone, true);
          }
        }
      }
    }

    ptr += mod_size;
  }

  return true;
}

bool RTTrPProtocol::parse_trackable(const uint8_t* data, std::size_t size, ParsedTrackable& out)
{
  if (size < 6)
    return false;

  const uint8_t* ptr = data;
  const uint8_t* end = data + size;

  // Read trackable name length (1 byte)
  uint8_t name_len = *ptr++;

  if (ptr + name_len > end)
    return false;

  // Read name
  out.name = std::string(reinterpret_cast<const char*>(ptr), name_len);
  ptr += name_len;

  // Read number of sub-modules (1 byte)
  if (ptr >= end)
    return false;

  uint8_t num_sub_modules = *ptr++;

  // Read timestamp (4 bytes)
  if (ptr + 4 > end)
    return false;

  out.timestamp = read_value<uint32_t>(ptr, !m_little_endian);

  // Parse sub-modules
  for (int i = 0; i < num_sub_modules && ptr < end; ++i)
  {
    if (ptr + 3 > end)
      break;

    uint8_t sub_type = *ptr++;
    uint16_t sub_size = read_value<uint16_t>(ptr, !m_little_endian);

    if (ptr + sub_size > end)
      break;

    const uint8_t* sub_ptr = ptr;

    switch (static_cast<ModuleType>(sub_type))
    {
      case ModuleType::CentroidMod:
      {
        // Centroid: latency (2), x (8), y (8), z (8) = 26 bytes
        if (sub_size >= 26)
        {
          out.latency = read_value<uint16_t>(sub_ptr, !m_little_endian);
          out.x = read_value<double>(sub_ptr, !m_little_endian);
          out.y = read_value<double>(sub_ptr, !m_little_endian);
          out.z = read_value<double>(sub_ptr, !m_little_endian);
          out.has_centroid = true;
        }
        break;
      }

      case ModuleType::QuatModule:
      {
        // Quaternion: latency (2), Qx (8), Qy (8), Qz (8), Qw (8) = 34 bytes
        if (sub_size >= 34)
        {
          uint16_t lat = read_value<uint16_t>(sub_ptr, !m_little_endian);
          out.qx = read_value<double>(sub_ptr, !m_little_endian);
          out.qy = read_value<double>(sub_ptr, !m_little_endian);
          out.qz = read_value<double>(sub_ptr, !m_little_endian);
          out.qw = read_value<double>(sub_ptr, !m_little_endian);
          out.has_quaternion = true;
        }
        break;
      }

      case ModuleType::EulerModule:
      {
        // Euler: latency (2), order (2), R1 (8), R2 (8), R3 (8) = 28 bytes
        if (sub_size >= 28)
        {
          uint16_t lat = read_value<uint16_t>(sub_ptr, !m_little_endian);
          out.euler_order = read_value<uint16_t>(sub_ptr, !m_little_endian);
          out.roll = read_value<double>(sub_ptr, !m_little_endian);
          out.pitch = read_value<double>(sub_ptr, !m_little_endian);
          out.yaw = read_value<double>(sub_ptr, !m_little_endian);
          out.has_euler = true;
        }
        break;
      }

      case ModuleType::CentroidAccVelMod:
      {
        // CentroidAccVel: x (8), y (8), z (8), ax (4), ay (4), az (4), vx (4), vy (4), vz (4) = 48 bytes
        if (sub_size >= 48)
        {
          out.x = read_value<double>(sub_ptr, !m_little_endian);
          out.y = read_value<double>(sub_ptr, !m_little_endian);
          out.z = read_value<double>(sub_ptr, !m_little_endian);
          out.ax = read_value<float>(sub_ptr, !m_little_endian);
          out.ay = read_value<float>(sub_ptr, !m_little_endian);
          out.az = read_value<float>(sub_ptr, !m_little_endian);
          out.vx = read_value<float>(sub_ptr, !m_little_endian);
          out.vy = read_value<float>(sub_ptr, !m_little_endian);
          out.vz = read_value<float>(sub_ptr, !m_little_endian);
          out.has_centroid = true;
          out.has_velocity = true;
        }
        break;
      }

      case ModuleType::LEDModule:
      {
        // LED: latency (2), index (1), x (8), y (8), z (8) = 27 bytes
        if (sub_size >= 27)
        {
          ParsedTrackable::LEDData led;
          led.latency = read_value<uint16_t>(sub_ptr, !m_little_endian);
          led.index = *sub_ptr++;
          led.x = read_value<double>(sub_ptr, !m_little_endian);
          led.y = read_value<double>(sub_ptr, !m_little_endian);
          led.z = read_value<double>(sub_ptr, !m_little_endian);
          out.leds.push_back(led);
        }
        break;
      }

      case ModuleType::LEDAccVelMod:
      {
        // LEDAccVel: index (1), x (8), y (8), z (8), ax (4), ay (4), az (4), vx (4), vy (4), vz (4) = 49 bytes
        if (sub_size >= 49)
        {
          ParsedTrackable::LEDData led;
          led.index = *sub_ptr++;
          led.x = read_value<double>(sub_ptr, !m_little_endian);
          led.y = read_value<double>(sub_ptr, !m_little_endian);
          led.z = read_value<double>(sub_ptr, !m_little_endian);
          led.ax = read_value<float>(sub_ptr, !m_little_endian);
          led.ay = read_value<float>(sub_ptr, !m_little_endian);
          led.az = read_value<float>(sub_ptr, !m_little_endian);
          led.vx = read_value<float>(sub_ptr, !m_little_endian);
          led.vy = read_value<float>(sub_ptr, !m_little_endian);
          led.vz = read_value<float>(sub_ptr, !m_little_endian);
          led.has_velocity = true;
          out.leds.push_back(led);
        }
        break;
      }

      case ModuleType::ZoneMod:
      {
        // Zone: num_zones (1), then zone_name_len (1), zone_name (n) for each
        if (sub_size >= 1)
        {
          uint8_t num_zones = *sub_ptr++;
          for (int z = 0; z < num_zones && sub_ptr < ptr + sub_size; ++z)
          {
            if (sub_ptr >= ptr + sub_size)
              break;
            uint8_t zone_len = *sub_ptr++;
            if (sub_ptr + zone_len <= ptr + sub_size)
            {
              out.zones.emplace_back(reinterpret_cast<const char*>(sub_ptr), zone_len);
              sub_ptr += zone_len;
            }
          }
        }
        break;
      }

      default:
        // Unknown module type, skip
        break;
    }

    ptr += sub_size;
  }

  return true;
}

void RTTrPProtocol::update_trackable_parameters(int slot, const ParsedTrackable& trackable)
{
  auto& root = m_device->get_root_node();

  auto* trackables_node = root.find_child(std::string_view("trackables"));
  if (!trackables_node)
    return;

  auto* slot_node = trackables_node->find_child(std::to_string(slot));
  if (!slot_node)
    return;

  using TB = Tracking::TreeBuilder;

  TB::update_param(slot_node, "active", true);
  TB::update_param(slot_node, "name", trackable.name);

  if (trackable.has_centroid)
  {
    TB::update_param(slot_node, "position",
                     ossia::vec3f{static_cast<float>(trackable.x),
                                  static_cast<float>(trackable.y),
                                  static_cast<float>(trackable.z)});
    TB::update_param(slot_node, "latency", (int)trackable.latency);
  }

  if (m_settings.enableQuaternion && trackable.has_quaternion)
  {
    TB::update_param(slot_node, "quaternion",
                     ossia::vec4f{static_cast<float>(trackable.qx),
                                  static_cast<float>(trackable.qy),
                                  static_cast<float>(trackable.qz),
                                  static_cast<float>(trackable.qw)});
  }

  if (m_settings.enableEuler && trackable.has_euler)
  {
    TB::update_param(slot_node, "euler",
                     ossia::vec3f{static_cast<float>(trackable.roll),
                                  static_cast<float>(trackable.pitch),
                                  static_cast<float>(trackable.yaw)});
  }

  if (m_settings.enableVelocity && trackable.has_velocity)
  {
    TB::update_param(slot_node, "velocity",
                     ossia::vec3f{trackable.vx, trackable.vy, trackable.vz});
  }

  if (m_settings.enableAcceleration && trackable.has_velocity)
  {
    TB::update_param(slot_node, "acceleration",
                     ossia::vec3f{trackable.ax, trackable.ay, trackable.az});
  }

  // Update LED markers
  if (!trackable.leds.empty())
  {
    auto* leds_node = slot_node->find_child(std::string_view("leds"));
    if (leds_node)
    {
      for (const auto& led : trackable.leds)
      {
        if (led.index >= m_settings.maxLEDsPerTrackable)
          continue;

        if (auto* led_node = leds_node->find_child(std::to_string(led.index)))
        {
          TB::update_param(led_node, "active", true);
          TB::update_param(led_node, "position",
                           ossia::vec3f{static_cast<float>(led.x),
                                        static_cast<float>(led.y),
                                        static_cast<float>(led.z)});

          if (led.has_velocity)
          {
            TB::update_param(led_node, "velocity",
                             ossia::vec3f{led.vx, led.vy, led.vz});
            TB::update_param(led_node, "acceleration",
                             ossia::vec3f{led.ax, led.ay, led.az});
          }
        }
      }
    }
  }
}

void RTTrPProtocol::update_zone_parameter(const std::string& zone_name, bool occupied)
{
  if (!m_settings.enableZones || zone_name.empty())
    return;

  auto& root = m_device->get_root_node();
  auto* zones_node = root.find_child(std::string_view("zones"));
  if (!zones_node)
    return;

  // Zone names come off the wire unvalidated; sanitize before using them as
  // ossia node names so spaces/slashes/non-ASCII don't produce invalid paths.
  const std::string safe_name = ossia::net::sanitize_name(zone_name);
  if (safe_name.empty())
    return;

  auto it = m_zone_nodes.find(safe_name);
  ossia::net::node_base* zone_node = nullptr;

  if (it == m_zone_nodes.end())
  {
    zone_node = Tracking::TreeBuilder::create_zone_slot(*zones_node, safe_name);
    m_zone_nodes[safe_name] = zone_node;
  }
  else
  {
    zone_node = it->second;
  }

  Tracking::TreeBuilder::update_param(zone_node, "occupied", occupied);
}

}
