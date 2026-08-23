// RTTrPM against the protocol definition.
//
// Packets are built here field by field, the way the specification lays them
// out (https://rttrp.github.io/RTTrP-Wiki/RTTrPM.html) and the way the
// reference decoders in 3rdparty/RTTrP read them back: the module "Size"
// fields include the type and size bytes, a 0x51 trackable carries its
// timestamp before its sub-module count, tracked point modules end with their
// index, and zone sub-modules start with their own size.

#include <RTTrP/RTTrPParser.hpp>
#include <RTTrP/RTTrPProtocol.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <catch2/catch_all.hpp>

#include <bit>
#include <cmath>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

using namespace RTTrP;

namespace
{
// ---- A spec-shaped packet builder --------------------------------------

struct Bytes
{
  std::vector<uint8_t> v;
  bool big_endian;

  explicit Bytes(bool be)
      : big_endian{be}
  {
  }

  template <typename T>
  void put(T value)
  {
    uint8_t raw[sizeof(T)];
    std::memcpy(raw, &value, sizeof(T));
    const bool host_be = std::endian::native == std::endian::big;
    if(sizeof(T) > 1 && host_be != big_endian)
      for(std::size_t i = 0; i < sizeof(T) / 2; ++i)
        std::swap(raw[i], raw[sizeof(T) - 1 - i]);
    v.insert(v.end(), raw, raw + sizeof(T));
  }
  void u8(uint8_t x) { v.push_back(x); }
  void u16(uint16_t x) { put(x); }
  void u32(uint32_t x) { put(x); }
  void f32(float x) { put(x); }
  void f64(double x) { put(x); }
  void str(const std::string& s) { v.insert(v.end(), s.begin(), s.end()); }

  // Patch a 16-bit size at `at` once a module is complete
  void size_at(std::size_t at, uint16_t size)
  {
    Bytes tmp{big_endian};
    tmp.u16(size);
    v[at] = tmp.v[0];
    v[at + 1] = tmp.v[1];
  }
};

// Module: Type u8, Size u16 (including both), payload written by `body`.
template <typename F>
void module(Bytes& b, uint8_t type, F&& body)
{
  const std::size_t start = b.v.size();
  b.u8(type);
  b.u16(0); // size, patched below
  body(b);
  b.size_at(start + 1, uint16_t(b.v.size() - start));
}

void centroid(Bytes& b, uint16_t latency, double x, double y, double z)
{
  module(b, 0x02, [&](Bytes& m) {
    m.u16(latency);
    m.f64(x);
    m.f64(y);
    m.f64(z);
  });
}
void quaternion(Bytes& b, uint16_t latency, double qx, double qy, double qz, double qw)
{
  module(b, 0x03, [&](Bytes& m) {
    m.u16(latency);
    m.f64(qx);
    m.f64(qy);
    m.f64(qz);
    m.f64(qw);
  });
}
void euler(Bytes& b, uint16_t latency, uint16_t order, double r1, double r2, double r3)
{
  module(b, 0x04, [&](Bytes& m) {
    m.u16(latency);
    m.u16(order);
    m.f64(r1);
    m.f64(r2);
    m.f64(r3);
  });
}
void led(Bytes& b, uint16_t latency, double x, double y, double z, uint8_t index)
{
  module(b, 0x06, [&](Bytes& m) {
    m.u16(latency);
    m.f64(x);
    m.f64(y);
    m.f64(z);
    m.u8(index);
  });
}
void centroid_accvel(
    Bytes& b, double x, double y, double z, float ax, float ay, float az, float vx,
    float vy, float vz)
{
  module(b, 0x20, [&](Bytes& m) {
    m.f64(x);
    m.f64(y);
    m.f64(z);
    m.f32(ax);
    m.f32(ay);
    m.f32(az);
    m.f32(vx);
    m.f32(vy);
    m.f32(vz);
  });
}
void led_accvel(
    Bytes& b, double x, double y, double z, float ax, float ay, float az, float vx,
    float vy, float vz, uint8_t index)
{
  module(b, 0x21, [&](Bytes& m) {
    m.f64(x);
    m.f64(y);
    m.f64(z);
    m.f32(ax);
    m.f32(ay);
    m.f32(az);
    m.f32(vx);
    m.f32(vy);
    m.f32(vz);
    m.u8(index);
  });
}
void zones(Bytes& b, const std::vector<std::string>& names)
{
  module(b, 0x22, [&](Bytes& m) {
    m.u8(uint8_t(names.size()));
    for(const auto& n : names)
    {
      m.u8(uint8_t(2 + n.size())); // size of the sub-module, itself included
      m.u8(uint8_t(n.size()));
      m.str(n);
    }
  });
}

// A trackable module with its sub-modules written by `subs`, which must
// return how many it wrote.
template <typename F>
void trackable(
    Bytes& b, const std::string& name, std::optional<uint32_t> timestamp, F&& subs)
{
  module(b, timestamp ? 0x51 : 0x01, [&](Bytes& m) {
    m.u8(uint8_t(name.size()));
    m.str(name);
    if(timestamp)
      m.u32(*timestamp);
    const std::size_t count_at = m.v.size();
    m.u8(0);
    const uint8_t n = subs(m);
    m.v[count_at] = n;
  });
}

// RTTrP header; `size` and `num_modules` patched by finish().
struct Packet
{
  Bytes b;
  explicit Packet(bool big_endian, uint32_t packet_id = 1, uint32_t context = 0)
      : b{big_endian}
  {
    b.u16(RTTRP_INT_HEADER_BIG);   // signature, in the packet's own byte order
    b.u16(RTTRPM_FLT_HEADER_BIG);  // idem
    b.u16(RTTRP_VERSION);
    b.u32(packet_id);
    b.u8(0); // raw
    b.u16(0); // size, patched
    b.u32(context);
    b.u8(0); // number of modules, patched
  }
  int modules{0};
  template <typename F>
  void add(F&& f)
  {
    f(b);
    modules++;
  }
  const std::vector<uint8_t>& finish()
  {
    b.size_at(11, uint16_t(b.v.size()));
    b.v[17] = uint8_t(modules);
    return b.v;
  }
};

ossia::value param(ossia::net::device_base& dev, std::string_view addr)
{
  auto n = ossia::net::find_node(dev.get_root_node(), addr);
  REQUIRE(n != nullptr);
  REQUIRE(n->get_parameter() != nullptr);
  return n->get_parameter()->value();
}
}

TEST_CASE("RTTrP header: signatures tell the byte order", "[tracking][rttrp]")
{
  for(bool be : {true, false})
  {
    Packet p{be, 0xDEADBEEF, 0x12345678};
    const auto& bytes = p.finish();
    REQUIRE(bytes.size() == RTTRP_HEADER_SIZE);

    // On the wire the big endian signature reads 41 54, the little endian one 54 41
    if(be)
      CHECK((bytes[0] == 0x41 && bytes[1] == 0x54));
    else
      CHECK((bytes[0] == 0x54 && bytes[1] == 0x41));

    PacketHeader h;
    REQUIRE(parse_header(bytes.data(), bytes.size(), h));
    CHECK(h.big_endian == be);
    CHECK(h.int_signature == RTTRP_INT_HEADER_BIG);
    CHECK(h.float_signature == RTTRPM_FLT_HEADER_BIG);
    CHECK(h.version == RTTRP_VERSION);
    CHECK(h.packet_id == 0xDEADBEEF);
    CHECK(h.format == 0);
    CHECK(h.size == RTTRP_HEADER_SIZE);
    CHECK(h.context == 0x12345678);
    CHECK(h.num_modules == 0);
  }

  // Short or not RTTrP at all
  PacketHeader h;
  uint8_t junk[RTTRP_HEADER_SIZE] = {'X', 'Y'};
  CHECK(!parse_header(junk, sizeof(junk), h));
  CHECK(!parse_header(junk, 4, h));
  CHECK(!parse_header(nullptr, 100, h));
}

TEST_CASE("RTTrP trackable: 0x01 has no timestamp, 0x51 has one before its module count", "[tracking][rttrp]")
{
  for(bool be : {true, false})
  {
    INFO((be ? "big endian" : "little endian"));
    Packet p{be, 7};
    p.add([](Bytes& b) {
      trackable(b, "Dancer", std::nullopt, [](Bytes& m) {
        centroid(m, 12, 1.5, -2.25, 3.125);
        return 1;
      });
    });
    p.add([](Bytes& b) {
      trackable(b, "Lamp", 0x01020304u, [](Bytes& m) {
        centroid(m, 0, 10, 20, 30);
        quaternion(m, 3, 0.1, 0.2, 0.3, 0.9);
        return 2;
      });
    });

    ParsedPacket out;
    const auto& bytes = p.finish();
    REQUIRE(parse_packet(bytes.data(), bytes.size(), out));
    REQUIRE(out.trackables.size() == 2);

    const auto& a = out.trackables[0];
    CHECK(a.name == "Dancer");
    CHECK(!a.has_timestamp);
    CHECK(a.has_centroid);
    CHECK(a.latency == 12);
    CHECK(a.x == 1.5);
    CHECK(a.y == -2.25);
    CHECK(a.z == 3.125);
    CHECK(!a.has_quaternion);

    const auto& l = out.trackables[1];
    CHECK(l.name == "Lamp");
    CHECK(l.has_timestamp);
    CHECK(l.timestamp == 0x01020304u);
    CHECK(l.has_centroid);
    CHECK(l.x == 10);
    CHECK(l.has_quaternion);
    CHECK(l.qx == 0.1);
    CHECK(l.qw == 0.9);
  }
}

TEST_CASE("RTTrP sub-modules: every kind, in spec order", "[tracking][rttrp]")
{
  Packet p{true, 3};
  p.add([](Bytes& b) {
    trackable(b, "T", 42u, [](Bytes& m) {
      euler(m, 5, 1, 0.5, 0.25, 0.125);
      led(m, 9, 1, 2, 3, 4);
      centroid_accvel(m, 7, 8, 9, 0.1f, 0.2f, 0.3f, 1.f, 2.f, 3.f);
      led_accvel(m, 11, 12, 13, 0.5f, 0.6f, 0.7f, 5.f, 6.f, 7.f, 2);
      zones(m, {"Stage", "Front of house"});
      return 5;
    });
  });

  ParsedPacket out;
  const auto& bytes = p.finish();
  REQUIRE(parse_packet(bytes.data(), bytes.size(), out));
  REQUIRE(out.trackables.size() == 1);
  const auto& t = out.trackables[0];

  CHECK(t.has_euler);
  CHECK(t.euler_order == 1);
  CHECK(t.roll == 0.5);
  CHECK(t.pitch == 0.25);
  CHECK(t.yaw == 0.125);
  CHECK(t.latency == 5);

  // Centroid from the accel/vel module
  CHECK(t.has_centroid);
  CHECK(t.x == 7);
  CHECK(t.has_velocity);
  CHECK(t.ax == 0.1f);
  CHECK(t.vz == 3.f);

  REQUIRE(t.leds.size() == 2);
  CHECK(t.leds[0].index == 4);
  CHECK(t.leds[0].x == 1);
  CHECK(t.leds[0].z == 3);
  CHECK(t.leds[0].latency == 9);
  CHECK(!t.leds[0].has_velocity);
  CHECK(t.leds[1].index == 2);
  CHECK(t.leds[1].x == 11);
  CHECK(t.leds[1].has_velocity);
  CHECK(t.leds[1].vy == 6.f);

  REQUIRE(t.zones.size() == 2);
  CHECK(t.zones[0] == "Stage");
  CHECK(t.zones[1] == "Front of house");
}

TEST_CASE("RTTrP: unknown modules are skipped by their size", "[tracking][rttrp]")
{
  Packet p{false, 5};
  // A lighting-ish module nobody knows about, then a trackable with an unknown sub-module
  p.add([](Bytes& b) {
    module(b, 0x7E, [](Bytes& m) {
      m.u32(0xAAAAAAAA);
      m.u8(1);
    });
  });
  p.add([](Bytes& b) {
    trackable(b, "X", std::nullopt, [](Bytes& m) {
      module(m, 0x7F, [](Bytes& mm) { mm.u16(0xBEEF); });
      centroid(m, 1, 4, 5, 6);
      return 2;
    });
  });

  ParsedPacket out;
  const auto& bytes = p.finish();
  REQUIRE(parse_packet(bytes.data(), bytes.size(), out));
  CHECK(out.unknown_modules == 1);
  REQUIRE(out.trackables.size() == 1);
  CHECK(out.trackables[0].has_centroid);
  CHECK(out.trackables[0].y == 5);
}

TEST_CASE("RTTrP: truncated packets do not read past the end", "[tracking][rttrp]")
{
  Packet p{true, 9};
  p.add([](Bytes& b) {
    trackable(b, "Long name here", 1u, [](Bytes& m) {
      centroid(m, 1, 1, 2, 3);
      quaternion(m, 1, 0, 0, 0, 1);
      return 2;
    });
  });
  const auto full = p.finish();

  // Every prefix parses without crashing; those shorter than the header fail
  for(std::size_t n = 0; n < full.size(); n++)
  {
    ParsedPacket out;
    const bool ok = parse_packet(full.data(), n, out);
    if(n < RTTRP_HEADER_SIZE)
      CHECK(!ok);
    else
      CHECK(ok);
  }

  // Header claiming more modules than present
  auto bytes = full;
  bytes[17] = 200;
  ParsedPacket out;
  REQUIRE(parse_packet(bytes.data(), bytes.size(), out));
  CHECK(out.trackables.size() == 1);
}

TEST_CASE("RTTrP device tree follows the packets", "[tracking][rttrp]")
{
  RTTrPSpecificSettings s;
  s.port = 0; // never bind the real port in a test
  s.numTrackables = 4;
  s.enableEuler = true;
  s.maxLEDsPerTrackable = 4;

  auto ctx = std::make_shared<ossia::net::network_context>();
  auto proto = std::make_unique<RTTrPProtocol>(ctx, s);
  auto& p = *proto;
  ossia::net::generic_device dev{std::move(proto), "rttrp"};

  Packet pk{true, 1};
  pk.add([](Bytes& b) {
    trackable(b, "Dancer", 1234u, [](Bytes& m) {
      centroid(m, 2, 1.0, 2.0, 3.0);
      quaternion(m, 2, 0, 0, 0, 1);
      euler(m, 2, 0, 0.1, 0.2, 0.3);
      centroid_accvel(m, 1.0, 2.0, 3.0, 0, 0, -9.8f, 1, 0, 0);
      led(m, 2, 5, 6, 7, 1);
      zones(m, {"Stage"});
      return 6;
    });
  });
  const auto& bytes = pk.finish();
  REQUIRE(p.process_packet(reinterpret_cast<const char*>(bytes.data()), bytes.size()));

  CHECK(param(dev, "frame") == ossia::value{1});
  CHECK(param(dev, "timestamp") == ossia::value{1234});
  CHECK(param(dev, "trackables/0/active") == ossia::value{true});
  CHECK(param(dev, "trackables/0/name") == ossia::value{std::string{"Dancer"}});
  CHECK(param(dev, "trackables/0/position") == ossia::value{ossia::vec3f{1, 2, 3}});
  CHECK(param(dev, "trackables/0/quaternion") == ossia::value{ossia::vec4f{0, 0, 0, 1}});
  CHECK(param(dev, "trackables/0/euler") == ossia::value{ossia::vec3f{0.1f, 0.2f, 0.3f}});
  CHECK(param(dev, "trackables/0/velocity") == ossia::value{ossia::vec3f{1, 0, 0}});
  CHECK(param(dev, "trackables/0/acceleration") == ossia::value{ossia::vec3f{0, 0, -9.8f}});
  CHECK(param(dev, "trackables/0/latency") == ossia::value{2});
  CHECK(param(dev, "trackables/0/leds/1/active") == ossia::value{true});
  CHECK(param(dev, "trackables/0/leds/1/position") == ossia::value{ossia::vec3f{5, 6, 7}});
  CHECK(param(dev, "zones/Stage/occupied") == ossia::value{true});

  // Same packet id again: a retransmit, ignored
  CHECK(!p.process_packet(reinterpret_cast<const char*>(bytes.data()), bytes.size()));

  // Another trackable lands in the next slot, the first keeps its own
  Packet pk2{false, 2};
  pk2.add([](Bytes& b) {
    trackable(b, "Lamp", std::nullopt, [](Bytes& m) {
      centroid(m, 0, 9, 9, 9);
      return 1;
    });
  });
  const auto& bytes2 = pk2.finish();
  REQUIRE(p.process_packet(reinterpret_cast<const char*>(bytes2.data()), bytes2.size()));
  CHECK(param(dev, "frame") == ossia::value{2});
  CHECK(param(dev, "trackables/1/name") == ossia::value{std::string{"Lamp"}});
  CHECK(param(dev, "trackables/1/position") == ossia::value{ossia::vec3f{9, 9, 9}});
  CHECK(param(dev, "trackables/0/position") == ossia::value{ossia::vec3f{1, 2, 3}});

  // Garbage is rejected
  CHECK(!p.process_packet("not rttrp at all, really not", 28));
}
