// TUIO 1.1 and 2.0, with OSC bundles written the way the specifications
// describe them:
//   TUIO 1.1  https://www.tuio.org/?specification
//             /tuio/2Dobj set s i x y a X Y A m r
//             /tuio/2Dcur set s x y X Y m
//             /tuio/2Dblb set s x y a w h f X Y A m r
//             bundle = [source] alive set* fseq
//   TUIO 2.0  https://www.tuio.org/?tuio20
//             /tuio2/frm f_id time dim source ... /tuio2/alv s_id*
//             /tuio2/tok s_id tu_id c_id x y angle [x_vel y_vel a_vel m_acc r_acc]
//             /tuio2/ptr s_id tu_id c_id x y angle shear radius press [...]
//             /tuio2/bnd s_id x y angle width height area [...]
//             /tuio2/sym s_id tu_id c_id group data

#include <TUIO/TUIOProtocol.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <oscpack/osc/OscOutboundPacketStream.h>

#include <catch2/catch_all.hpp>

#include <cmath>
#include <memory>
#include <string>

namespace
{
ossia::value param(ossia::net::device_base& dev, std::string_view addr)
{
  auto n = ossia::net::find_node(dev.get_root_node(), addr);
  REQUIRE(n != nullptr);
  REQUIRE(n->get_parameter() != nullptr);
  return n->get_parameter()->value();
}

struct Fixture
{
  std::shared_ptr<ossia::net::network_context> ctx;
  TUIO::TUIOProtocol* proto{};
  std::unique_ptr<ossia::net::generic_device> dev;
  char buffer[4096];

  explicit Fixture(TUIO::TUIOVersion v = TUIO::TUIOVersion::V1_1, int slots = 4)
  {
    ctx = std::make_shared<ossia::net::network_context>();
    // Port 0: never bind the real TUIO port in a test
    auto p = std::make_unique<TUIO::TUIOProtocol>(ctx, 0, slots, slots, slots, v);
    proto = p.get();
    dev = std::make_unique<ossia::net::generic_device>(std::move(p), "tuio");
  }

  oscpack::OutboundPacketStream stream() { return oscpack::OutboundPacketStream{buffer, sizeof(buffer)}; }
  void send(const oscpack::OutboundPacketStream& s) { proto->process_packet(s.Data(), s.Size()); }
};
}

// ---- TUIO 1.1 ---------------------------------------------------------------

TEST_CASE("TUIO 1.1 cursor bundle: source, alive, set, fseq", "[tracking][tuio]")
{
  Fixture f;
  auto s = f.stream();
  s << oscpack::BeginBundleImmediate()
    << oscpack::BeginMessage("/tuio/2Dcur") << "source" << "reacTIVision@192.168.0.7" << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio/2Dcur") << "alive" << int32_t(3) << int32_t(4) << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio/2Dcur") << "set" << int32_t(3) << 0.25f << 0.75f << 1.f << -0.5f << 2.f << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio/2Dcur") << "set" << int32_t(4) << 0.5f << 0.5f << 0.f << 0.f << 0.f << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio/2Dcur") << "fseq" << int32_t(42) << oscpack::EndMessage()
    << oscpack::EndBundle();
  f.send(s);

  CHECK(param(*f.dev, "source") == ossia::value{std::string{"reacTIVision@192.168.0.7"}});
  CHECK(param(*f.dev, "frame") == ossia::value{42});

  CHECK(param(*f.dev, "2Dcur/0/active") == ossia::value{true});
  CHECK(param(*f.dev, "2Dcur/0/session_id") == ossia::value{3});
  CHECK(param(*f.dev, "2Dcur/0/position") == ossia::value{ossia::vec2f{0.25f, 0.75f}});
  CHECK(param(*f.dev, "2Dcur/0/velocity") == ossia::value{ossia::vec2f{1.f, -0.5f}});
  CHECK(param(*f.dev, "2Dcur/0/motion_acceleration") == ossia::value{2.f});

  CHECK(param(*f.dev, "2Dcur/1/session_id") == ossia::value{4});
  CHECK(param(*f.dev, "2Dcur/1/position") == ossia::value{ossia::vec2f{0.5f, 0.5f}});
  CHECK(param(*f.dev, "2Dcur/2/active") == ossia::value{false});

  // Next bundle: cursor 3 lifted, only 4 alive -> slot 0 freed, slot 1 kept
  auto s2 = f.stream();
  s2 << oscpack::BeginBundleImmediate()
     << oscpack::BeginMessage("/tuio/2Dcur") << "alive" << int32_t(4) << oscpack::EndMessage()
     << oscpack::BeginMessage("/tuio/2Dcur") << "set" << int32_t(4) << 0.6f << 0.4f << 0.f << 0.f << 0.f << oscpack::EndMessage()
     << oscpack::BeginMessage("/tuio/2Dcur") << "fseq" << int32_t(43) << oscpack::EndMessage()
     << oscpack::EndBundle();
  f.send(s2);
  CHECK(param(*f.dev, "frame") == ossia::value{43});
  CHECK(param(*f.dev, "2Dcur/0/active") == ossia::value{false});
  CHECK(param(*f.dev, "2Dcur/1/active") == ossia::value{true});
  CHECK(param(*f.dev, "2Dcur/1/position") == ossia::value{ossia::vec2f{0.6f, 0.4f}});

  // A new cursor takes the freed slot
  auto s3 = f.stream();
  s3 << oscpack::BeginBundleImmediate()
     << oscpack::BeginMessage("/tuio/2Dcur") << "alive" << int32_t(4) << int32_t(9) << oscpack::EndMessage()
     << oscpack::BeginMessage("/tuio/2Dcur") << "set" << int32_t(9) << 0.1f << 0.1f << 0.f << 0.f << 0.f << oscpack::EndMessage()
     << oscpack::BeginMessage("/tuio/2Dcur") << "fseq" << int32_t(44) << oscpack::EndMessage()
     << oscpack::EndBundle();
  f.send(s3);
  CHECK(param(*f.dev, "2Dcur/0/session_id") == ossia::value{9});
  CHECK(param(*f.dev, "2Dcur/0/active") == ossia::value{true});
}

TEST_CASE("TUIO 1.1 object set: s i x y a X Y A m r", "[tracking][tuio]")
{
  Fixture f;
  auto s = f.stream();
  s << oscpack::BeginBundleImmediate()
    << oscpack::BeginMessage("/tuio/2Dobj") << "alive" << int32_t(11) << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio/2Dobj") << "set" << int32_t(11) << int32_t(5)
      << 0.3f << 0.7f << 1.5f << 0.1f << 0.2f << 0.3f << 0.4f << 0.5f << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio/2Dobj") << "fseq" << int32_t(1) << oscpack::EndMessage()
    << oscpack::EndBundle();
  f.send(s);

  CHECK(param(*f.dev, "2Dobj/0/active") == ossia::value{true});
  CHECK(param(*f.dev, "2Dobj/0/session_id") == ossia::value{11});
  CHECK(param(*f.dev, "2Dobj/0/class_id") == ossia::value{5});
  CHECK(param(*f.dev, "2Dobj/0/position") == ossia::value{ossia::vec2f{0.3f, 0.7f}});
  CHECK(param(*f.dev, "2Dobj/0/angle") == ossia::value{1.5f});
  CHECK(param(*f.dev, "2Dobj/0/velocity") == ossia::value{ossia::vec2f{0.1f, 0.2f}});
  CHECK(param(*f.dev, "2Dobj/0/angle_velocity") == ossia::value{0.3f});
  CHECK(param(*f.dev, "2Dobj/0/motion_acceleration") == ossia::value{0.4f});
  CHECK(param(*f.dev, "2Dobj/0/rotation_acceleration") == ossia::value{0.5f});
}

TEST_CASE("TUIO 1.1 blob set: s x y a w h f X Y A m r", "[tracking][tuio]")
{
  Fixture f;
  auto s = f.stream();
  s << oscpack::BeginBundleImmediate()
    << oscpack::BeginMessage("/tuio/2Dblb") << "alive" << int32_t(2) << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio/2Dblb") << "set" << int32_t(2)
      << 0.5f << 0.25f << 0.75f << 0.2f << 0.1f << 0.02f << 1.f << 2.f << 3.f << 4.f << 5.f << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio/2Dblb") << "fseq" << int32_t(1) << oscpack::EndMessage()
    << oscpack::EndBundle();
  f.send(s);

  CHECK(param(*f.dev, "2Dblb/0/session_id") == ossia::value{2});
  CHECK(param(*f.dev, "2Dblb/0/position") == ossia::value{ossia::vec2f{0.5f, 0.25f}});
  CHECK(param(*f.dev, "2Dblb/0/angle") == ossia::value{0.75f});
  CHECK(param(*f.dev, "2Dblb/0/size") == ossia::value{ossia::vec2f{0.2f, 0.1f}});
  CHECK(param(*f.dev, "2Dblb/0/area") == ossia::value{0.02f});
  CHECK(param(*f.dev, "2Dblb/0/velocity") == ossia::value{ossia::vec2f{1.f, 2.f}});
  CHECK(param(*f.dev, "2Dblb/0/angle_velocity") == ossia::value{3.f});
  CHECK(param(*f.dev, "2Dblb/0/motion_acceleration") == ossia::value{4.f});
  CHECK(param(*f.dev, "2Dblb/0/rotation_acceleration") == ossia::value{5.f});
}

TEST_CASE("TUIO 1.1 ignores malformed and TUIO 2.0 messages", "[tracking][tuio]")
{
  Fixture f;
  auto s = f.stream();
  s << oscpack::BeginBundleImmediate()
    // Too few arguments for a cursor set
    << oscpack::BeginMessage("/tuio/2Dcur") << "set" << int32_t(1) << 0.5f << oscpack::EndMessage()
    // Wrong types
    << oscpack::BeginMessage("/tuio/2Dobj") << "set" << "oops" << oscpack::EndMessage()
    // A TUIO 2.0 message on a 1.1 device
    << oscpack::BeginMessage("/tuio2/frm") << int32_t(1) << oscpack::TimeTag(1) << int32_t(0) << "x" << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio/2Dcur") << "fseq" << int32_t(5) << oscpack::EndMessage()
    << oscpack::EndBundle();
  f.send(s);
  CHECK(param(*f.dev, "frame") == ossia::value{5});
  CHECK(param(*f.dev, "2Dcur/0/active") == ossia::value{false});
  CHECK(param(*f.dev, "2Dobj/0/active") == ossia::value{false});

  // Plain non-OSC bytes
  f.proto->process_packet("hello there", 11);
  SUCCEED("no crash on garbage");
}

// ---- TUIO 2.0 ---------------------------------------------------------------

TEST_CASE("TUIO 2.0 frame, token, pointer, bounds and alive", "[tracking][tuio]")
{
  Fixture f{TUIO::TUIOVersion::V2_0};
  const int32_t dim = (640 << 16) | 480;
  // tu_id: user id in the upper 16 bits, type id in the lower 16
  const int32_t tu_id = (3 << 16) | 42;

  auto s = f.stream();
  s << oscpack::BeginBundleImmediate()
    << oscpack::BeginMessage("/tuio2/frm") << int32_t(100) << oscpack::TimeTag(1) << dim << "tuio2demo:1@localhost" << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio2/tok") << int32_t(1) << tu_id << int32_t(7) << 0.25f << 0.5f << 1.f
      << 0.1f << 0.2f << 0.3f << 0.4f << 0.5f << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio2/ptr") << int32_t(2) << int32_t(1) << int32_t(0) << 0.6f << 0.7f << 0.f
      << 0.1f << 0.05f << -1.f << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio2/bnd") << int32_t(3) << 0.4f << 0.3f << 0.2f << 0.1f << 0.05f << 0.01f << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio2/sym") << int32_t(1) << tu_id << int32_t(7) << "fiducial" << "42" << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio2/alv") << int32_t(1) << int32_t(2) << int32_t(3) << oscpack::EndMessage()
    << oscpack::EndBundle();
  f.send(s);

  CHECK(param(*f.dev, "frame") == ossia::value{100});
  CHECK(param(*f.dev, "source") == ossia::value{std::string{"tuio2demo:1@localhost"}});

  CHECK(param(*f.dev, "tok/0/active") == ossia::value{true});
  CHECK(param(*f.dev, "tok/0/session_id") == ossia::value{1});
  CHECK(param(*f.dev, "tok/0/user_id") == ossia::value{3});
  CHECK(param(*f.dev, "tok/0/type_id") == ossia::value{42});
  CHECK(param(*f.dev, "tok/0/component_id") == ossia::value{7});
  CHECK(param(*f.dev, "tok/0/position") == ossia::value{ossia::vec2f{0.25f, 0.5f}});
  CHECK(param(*f.dev, "tok/0/angle") == ossia::value{1.f});
  CHECK(param(*f.dev, "tok/0/velocity") == ossia::value{ossia::vec2f{0.1f, 0.2f}});
  CHECK(param(*f.dev, "tok/0/angle_velocity") == ossia::value{0.3f});
  CHECK(param(*f.dev, "tok/0/motion_acceleration") == ossia::value{0.4f});
  CHECK(param(*f.dev, "tok/0/rotation_acceleration") == ossia::value{0.5f});
  CHECK(param(*f.dev, "tok/0/symbol_group") == ossia::value{std::string{"fiducial"}});
  CHECK(param(*f.dev, "tok/0/symbol_data") == ossia::value{std::string{"42"}});

  CHECK(param(*f.dev, "ptr/0/active") == ossia::value{true});
  CHECK(param(*f.dev, "ptr/0/session_id") == ossia::value{2});
  CHECK(param(*f.dev, "ptr/0/position") == ossia::value{ossia::vec2f{0.6f, 0.7f}});
  CHECK(param(*f.dev, "ptr/0/shear") == ossia::value{0.1f});
  CHECK(param(*f.dev, "ptr/0/radius") == ossia::value{0.05f});
  // Negative pressure: hovering
  CHECK(param(*f.dev, "ptr/0/pressure") == ossia::value{-1.f});

  CHECK(param(*f.dev, "bnd/0/active") == ossia::value{true});
  CHECK(param(*f.dev, "bnd/0/position") == ossia::value{ossia::vec2f{0.4f, 0.3f}});
  CHECK(param(*f.dev, "bnd/0/angle") == ossia::value{0.2f});
  CHECK(param(*f.dev, "bnd/0/size") == ossia::value{ossia::vec2f{0.1f, 0.05f}});
  CHECK(param(*f.dev, "bnd/0/area") == ossia::value{0.01f});

  // Next frame: everything but the pointer is gone
  auto s2 = f.stream();
  s2 << oscpack::BeginBundleImmediate()
     << oscpack::BeginMessage("/tuio2/frm") << int32_t(101) << oscpack::TimeTag(1) << dim << "tuio2demo:1@localhost" << oscpack::EndMessage()
     << oscpack::BeginMessage("/tuio2/alv") << int32_t(2) << oscpack::EndMessage()
     << oscpack::EndBundle();
  f.send(s2);
  CHECK(param(*f.dev, "frame") == ossia::value{101});
  CHECK(param(*f.dev, "tok/0/active") == ossia::value{false});
  CHECK(param(*f.dev, "bnd/0/active") == ossia::value{false});
  CHECK(param(*f.dev, "ptr/0/active") == ossia::value{true});
}

TEST_CASE("TUIO 2.0 optional velocity block may be omitted", "[tracking][tuio]")
{
  Fixture f{TUIO::TUIOVersion::V2_0};
  auto s = f.stream();
  s << oscpack::BeginBundleImmediate()
    << oscpack::BeginMessage("/tuio2/frm") << int32_t(1) << oscpack::TimeTag(1) << int32_t(0) << "src" << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio2/tok") << int32_t(5) << int32_t(0) << int32_t(0) << 0.1f << 0.2f << 0.3f << oscpack::EndMessage()
    << oscpack::BeginMessage("/tuio2/alv") << int32_t(5) << oscpack::EndMessage()
    << oscpack::EndBundle();
  f.send(s);
  CHECK(param(*f.dev, "tok/0/position") == ossia::value{ossia::vec2f{0.1f, 0.2f}});
  CHECK(param(*f.dev, "tok/0/angle") == ossia::value{0.3f});
  // Untouched defaults
  CHECK(param(*f.dev, "tok/0/velocity") == ossia::value{ossia::vec2f{0.f, 0.f}});
}
