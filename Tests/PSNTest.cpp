// PosiStageNet, with packets produced by VYV's official psn-cpp encoder
// (3rdparty/psn-cpp, the same library whose decoder the protocol uses), so
// that what the device tree shows is checked against what the reference
// implementation put on the wire.

#include <PSN/PSNProtocol.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <catch2/catch_all.hpp>

// The encoder is header-only with out-of-line definitions: one TU includes it.
#include <psn_encoder.hpp>
#include <psn_encoder_impl.hpp>

#include <list>
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
  PSN::PSNSpecificSettings settings;
  std::shared_ptr<ossia::net::network_context> ctx;
  PSN::PSNProtocol* proto{};
  std::unique_ptr<ossia::net::generic_device> dev;

  explicit Fixture(int trackers = 8)
  {
    settings.port = 0; // never bind the real port in a test
    settings.numTrackers = trackers;
    settings.enableTargetPosition = true;
    ctx = std::make_shared<ossia::net::network_context>();
    auto p = std::make_unique<PSN::PSNProtocol>(ctx, settings);
    proto = p.get();
    dev = std::make_unique<ossia::net::generic_device>(std::move(p), "psn");
  }

  int feed(const std::list<std::string>& packets)
  {
    int accepted = 0;
    for(const auto& pkt : packets)
      accepted += proto->process_packet(pkt.data(), pkt.size()) ? 1 : 0;
    return accepted;
  }
};

psn::tracker_map two_trackers()
{
  psn::tracker_map trackers;
  {
    psn::tracker t{1, "Dancer"};
    t.set_pos({1.f, 2.f, 3.f});
    t.set_speed({0.5f, 0.f, -0.5f});
    t.set_ori({0.1f, 0.2f, 0.3f});
    t.set_accel({0.f, 0.f, -9.8f});
    t.set_status(0.75f);
    t.set_target_pos({10.f, 20.f, 30.f});
    t.set_timestamp(5'000'000); // 5 s in microseconds
    trackers[t.get_id()] = t;
  }
  {
    psn::tracker t{7, "Lamp"};
    t.set_pos({-1.f, 0.f, 4.f});
    trackers[t.get_id()] = t;
  }
  return trackers;
}
}

TEST_CASE("PSN: the tree starts empty and correctly shaped", "[tracking][psn]")
{
  Fixture f{3};
  CHECK(param(*f.dev, "system_name") == ossia::value{std::string{}});
  CHECK(param(*f.dev, "trackers/0/active") == ossia::value{false});
  CHECK(param(*f.dev, "trackers/2/id") == ossia::value{-1});
  CHECK(ossia::net::find_node(f.dev->get_root_node(), "trackers/3") == nullptr);
}

TEST_CASE("PSN info packets name the system and the trackers", "[tracking][psn]")
{
  Fixture f;
  psn::psn_encoder enc{"Test System"};
  const auto packets = enc.encode_info(two_trackers(), 1'000'000);
  REQUIRE(!packets.empty());
  CHECK(f.feed(packets) == int(packets.size()));

  CHECK(param(*f.dev, "system_name") == ossia::value{std::string{"Test System"}});
  // Trackers are slotted in id order of arrival; the info packet lists 1 then 7
  CHECK(param(*f.dev, "trackers/0/id") == ossia::value{1});
  CHECK(param(*f.dev, "trackers/0/name") == ossia::value{std::string{"Dancer"}});
  CHECK(param(*f.dev, "trackers/1/id") == ossia::value{7});
  CHECK(param(*f.dev, "trackers/1/name") == ossia::value{std::string{"Lamp"}});
  CHECK(param(*f.dev, "timestamp") == ossia::value{1000}); // ms
}

TEST_CASE("PSN data packets drive the tracker slots", "[tracking][psn]")
{
  Fixture f;
  psn::psn_encoder enc{"Test System"};
  f.feed(enc.encode_info(two_trackers(), 1'000'000));
  const auto packets = enc.encode_data(two_trackers(), 2'000'000);
  REQUIRE(!packets.empty());
  CHECK(f.feed(packets) == int(packets.size()));

  CHECK(param(*f.dev, "timestamp") == ossia::value{2000});
  CHECK(param(*f.dev, "trackers/0/active") == ossia::value{true});
  CHECK(param(*f.dev, "trackers/0/id") == ossia::value{1});
  CHECK(param(*f.dev, "trackers/0/position") == ossia::value{ossia::vec3f{1, 2, 3}});
  CHECK(param(*f.dev, "trackers/0/velocity") == ossia::value{ossia::vec3f{0.5f, 0, -0.5f}});
  CHECK(param(*f.dev, "trackers/0/orientation") == ossia::value{ossia::vec3f{0.1f, 0.2f, 0.3f}});
  CHECK(param(*f.dev, "trackers/0/acceleration") == ossia::value{ossia::vec3f{0, 0, -9.8f}});
  CHECK(param(*f.dev, "trackers/0/status") == ossia::value{0.75f});
  CHECK(param(*f.dev, "trackers/0/target_position") == ossia::value{ossia::vec3f{10, 20, 30}});
  CHECK(param(*f.dev, "trackers/0/timestamp") == ossia::value{5000});

  CHECK(param(*f.dev, "trackers/1/active") == ossia::value{true});
  CHECK(param(*f.dev, "trackers/1/id") == ossia::value{7});
  CHECK(param(*f.dev, "trackers/1/position") == ossia::value{ossia::vec3f{-1, 0, 4}});
  // Never sent for the lamp: left at its default
  CHECK(param(*f.dev, "trackers/1/status") == ossia::value{0.f});

  CHECK(param(*f.dev, "trackers/2/active") == ossia::value{false});
}

TEST_CASE("PSN trackers keep their slot across frames, ids being persistent", "[tracking][psn]")
{
  Fixture f;
  psn::psn_encoder enc{"S"};
  f.feed(enc.encode_data(two_trackers(), 1'000'000));

  // Next frame: only the lamp, moved
  psn::tracker_map only_lamp;
  {
    psn::tracker t{7, "Lamp"};
    t.set_pos({5.f, 5.f, 5.f});
    only_lamp[7] = t;
  }
  f.feed(enc.encode_data(only_lamp, 1'100'000));
  CHECK(param(*f.dev, "trackers/1/position") == ossia::value{ossia::vec3f{5, 5, 5}});
  CHECK(param(*f.dev, "trackers/1/id") == ossia::value{7});
  // The dancer's slot is untouched
  CHECK(param(*f.dev, "trackers/0/position") == ossia::value{ossia::vec3f{1, 2, 3}});
}

TEST_CASE("PSN rejects retransmits and garbage", "[tracking][psn]")
{
  Fixture f;
  psn::psn_encoder enc{"S"};
  const auto packets = enc.encode_data(two_trackers(), 1'000'000);
  REQUIRE(f.feed(packets) == int(packets.size()));
  // The very same frame again: same frame id, dropped
  CHECK(f.feed(packets) == 0);

  CHECK(!f.proto->process_packet("definitely not posistagenet", 27));
  CHECK(!f.proto->process_packet("", 0));
}

TEST_CASE("PSN: more trackers than slots do not write past the tree", "[tracking][psn]")
{
  Fixture f{2};
  psn::psn_encoder enc{"S"};
  psn::tracker_map many;
  for(uint16_t i = 0; i < 6; i++)
  {
    psn::tracker t{i, "T" + std::to_string(i)};
    t.set_pos({float(i), 0.f, 0.f});
    many[i] = t;
  }
  const auto packets = enc.encode_data(many, 1'000'000);
  f.feed(packets);
  CHECK(ossia::net::find_node(f.dev->get_root_node(), "trackers/2") == nullptr);
  CHECK(param(*f.dev, "trackers/0/active") == ossia::value{true});
  CHECK(param(*f.dev, "trackers/1/active") == ossia::value{true});
}

TEST_CASE("PSN: a frame split over several packets is applied whole", "[tracking][psn]")
{
  // The encoder splits a frame over as many UDP packets as needed to stay
  // under psn::MAX_UDP_PACKET_SIZE (1500 bytes): with 40 trackers that is
  // more than one. The tree must only change once the frame is complete, and
  // then hold every tracker.
  Fixture f{64};
  psn::psn_encoder enc{"Big stage"};
  psn::tracker_map many;
  for(uint16_t i = 0; i < 40; i++)
  {
    psn::tracker t{i, "Tracker " + std::to_string(i)};
    t.set_pos({float(i), float(2 * i), float(3 * i)});
    t.set_speed({1.f, 0.f, 0.f});
    t.set_ori({0.f, 0.f, 0.f});
    t.set_accel({0.f, 0.f, 0.f});
    t.set_status(1.f);
    t.set_timestamp(1000 * i);
    many[i] = t;
  }
  const auto packets = enc.encode_data(many, 1'000'000);
  REQUIRE(packets.size() > 1);
  for(const auto& p : packets)
    CHECK(p.size() <= psn::MAX_UDP_PACKET_SIZE);

  // Applied exactly once, by the packet that completes the frame
  CHECK(f.feed(packets) == 1);
  for(int i = 0; i < 40; i++)
  {
    INFO("tracker " << i);
    CHECK(param(*f.dev, "trackers/" + std::to_string(i) + "/active") == ossia::value{true});
    CHECK(param(*f.dev, "trackers/" + std::to_string(i) + "/id") == ossia::value{i});
    CHECK(param(*f.dev, "trackers/" + std::to_string(i) + "/position")
          == ossia::value{ossia::vec3f{float(i), float(2 * i), float(3 * i)}});
  }

  // Names come with the info frame, also split
  const auto info = enc.encode_info(many, 1'100'000);
  REQUIRE(info.size() >= 1);
  CHECK(f.feed(info) == 1);
  CHECK(param(*f.dev, "trackers/39/name") == ossia::value{std::string{"Tracker 39"}});
}
