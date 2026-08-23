// OpenTrackIO 1.0.1:
//  - the OTrk transport header and its Fletcher-16 checksum, as specified at
//    https://ris-pub.smpte.org/ris-osvp-metadata-camdkit/ ;
//  - the samples published with the specification (data/opentrackio/*.json,
//    fetched from .../examples/), decoded by the vendored Mo-Sys parser and
//    applied to the device tree.

#include <OpenTrackIO/OTrkHeader.hpp>
#include <OpenTrackIO/OpenTrackIOProtocol.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <catch2/catch_all.hpp>

#include <cmath>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace OpenTrackIO;

namespace
{
// The reference implementation given in the specification, verbatim
uint16_t spec_fletcher(const uint8_t* data, uint16_t len)
{
  if(!data)
    return 0;
  uint8_t sum1 = 0, sum2 = 0;
  while(len-- > 0)
  {
    sum1 += *data++;
    sum2 += sum1;
  }
  return ((uint16_t)sum2 << 8) | (uint16_t)sum1;
}

std::vector<uint8_t> datagram(
    const std::string& payload, uint8_t encoding = OTRK_ENC_JSON, uint16_t sequence = 1,
    uint32_t offset = 0, bool last = true)
{
  std::vector<uint8_t> d(OTRK_HEADER_LEN + payload.size());
  otrk_write_header(
      d.data(), encoding, sequence, offset, last,
      reinterpret_cast<const uint8_t*>(payload.data()), uint16_t(payload.size()));
  std::copy(payload.begin(), payload.end(), d.begin() + OTRK_HEADER_LEN);
  return d;
}

std::string read_file(const std::string& name)
{
  std::ifstream f{std::string{TRACKING_TEST_DATA} + "/opentrackio/" + name};
  REQUIRE(f.good());
  return std::string{std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
}

ossia::value param(ossia::net::device_base& dev, std::string_view addr)
{
  auto n = ossia::net::find_node(dev.get_root_node(), addr);
  REQUIRE(n != nullptr);
  REQUIRE(n->get_parameter() != nullptr);
  return n->get_parameter()->value();
}

bool approx_eq(const ossia::value& v, float expected)
{
  return std::abs(v.get<float>() - expected) < 1e-4f;
}

struct Fixture
{
  std::shared_ptr<ossia::net::network_context> ctx;
  OpenTrackIOProtocol* proto{};
  std::unique_ptr<ossia::net::generic_device> dev;

  Fixture()
  {
    OpenTrackIOSpecificSettings s;
    s.port = 0; // never bind the real port in a test
    s.minSourceNumber = 1;
    s.maxSourceNumber = 1;
    s.enableGlobalStage = true;
    s.acceptCBOR = true;
    ctx = std::make_shared<ossia::net::network_context>();
    auto p = std::make_unique<OpenTrackIOProtocol>(ctx, s);
    proto = p.get();
    dev = std::make_unique<ossia::net::generic_device>(std::move(p), "otio");
  }

  bool feed(int source, const std::vector<uint8_t>& d)
  {
    return proto->process_datagram(source, reinterpret_cast<const char*>(d.data()), d.size());
  }
};
}

TEST_CASE("OTrk: Fletcher-16 matches the specification's reference", "[tracking][opentrackio]")
{
  // "abcde": sum1 = 97,195,38,138,239 and sum2 = 97,36,74,212,195 with the
  // modulus-256 arithmetic the specification mandates -> 0xC3EF. (The textbook
  // Fletcher-16, modulus 255, gives 0xC8F0 for the same input: not this one.)
  const std::string abcde = "abcde";
  CHECK(fletcher16(reinterpret_cast<const uint8_t*>(abcde.data()), abcde.size()) == 0xC3EF);
  CHECK(spec_fletcher(reinterpret_cast<const uint8_t*>(abcde.data()), abcde.size()) == 0xC3EF);

  // Against the reference on varied buffers, including the split-range variant
  std::vector<uint8_t> buf(1000);
  uint32_t x = 12345;
  for(auto& b : buf)
  {
    x = x * 1103515245u + 12345u;
    b = uint8_t(x >> 16);
  }
  for(std::size_t len : {0u, 1u, 2u, 17u, 255u, 256u, 999u})
  {
    CHECK(fletcher16(buf.data(), len) == spec_fletcher(buf.data(), uint16_t(len)));
    const std::size_t cut = len / 3;
    CHECK(fletcher16(buf.data(), cut, buf.data() + cut, len - cut) == spec_fletcher(buf.data(), uint16_t(len)));
  }
  CHECK(fletcher16(nullptr, 10) == 0);
}

TEST_CASE("OTrk header round-trips and is validated", "[tracking][opentrackio]")
{
  const std::string payload = R"({"protocol":{"name":"OpenTrackIO","version":[1,0,1]}})";
  auto d = datagram(payload, OTRK_ENC_JSON, 0x1234, 0, true);
  REQUIRE(d.size() == 16 + payload.size());

  // Byte layout per the specification
  CHECK(std::string(d.begin(), d.begin() + 4) == "OTrk");
  CHECK(d[4] == 0);
  CHECK(d[5] == 0x01);
  CHECK((d[6] == 0x12 && d[7] == 0x34));
  CHECK((d[8] == 0 && d[9] == 0 && d[10] == 0 && d[11] == 0));
  CHECK((d[12] & 0x80) != 0); // last segment
  CHECK(((d[12] & 0x7F) << 8 | d[13]) == int(payload.size()));
  // Checksum: Fletcher-16 over the header minus its checksum bytes, then the payload
  std::vector<uint8_t> covered(d.begin(), d.begin() + 14);
  covered.insert(covered.end(), d.begin() + 16, d.end());
  CHECK(((d[14] << 8) | d[15]) == spec_fletcher(covered.data(), uint16_t(covered.size())));

  OTrkHeader h;
  REQUIRE(otrk_parse_header(reinterpret_cast<const char*>(d.data()), d.size(), h));
  CHECK(h.encoding == OTRK_ENC_JSON);
  CHECK(h.sequence == 0x1234);
  CHECK(h.segment_offset == 0);
  CHECK(h.last_segment);
  CHECK(h.payload_length == payload.size());

  // Not last, with an offset
  auto d2 = datagram(payload, OTRK_ENC_CBOR, 7, 500, false);
  REQUIRE(otrk_parse_header(reinterpret_cast<const char*>(d2.data()), d2.size(), h));
  CHECK(h.encoding == OTRK_ENC_CBOR);
  CHECK(h.segment_offset == 500);
  CHECK(!h.last_segment);

  // Rejections: short, bad magic, unknown encoding, bad length, bad checksum
  CHECK(!otrk_parse_header(reinterpret_cast<const char*>(d.data()), 10, h));
  auto bad = d;
  bad[0] = 'X';
  CHECK(!otrk_parse_header(reinterpret_cast<const char*>(bad.data()), bad.size(), h));
  bad = d;
  bad[5] = 0x03;
  CHECK(!otrk_parse_header(reinterpret_cast<const char*>(bad.data()), bad.size(), h));
  bad = d;
  bad[13] = uint8_t(bad[13] + 1); // claims one more payload byte than present
  CHECK(!otrk_parse_header(reinterpret_cast<const char*>(bad.data()), bad.size(), h));
  bad = d;
  bad[20] ^= 0xFF; // payload corrupted: checksum fails
  CHECK(!otrk_parse_header(reinterpret_cast<const char*>(bad.data()), bad.size(), h));
  bad = d;
  bad[4] = 0x55; // reserved byte: covered by the checksum, but its value is ignored
  otrk_write_u16_be(bad.data() + 14, fletcher16(bad.data(), 14, bad.data() + 16, uint16_t(payload.size())));
  CHECK(otrk_parse_header(reinterpret_cast<const char*>(bad.data()), bad.size(), h));
}

TEST_CASE("OpenTrackIO: the recommended dynamic example drives the tree", "[tracking][opentrackio]")
{
  Fixture f;
  const auto json = read_file("recommended_dynamic_example.json");
  REQUIRE(f.feed(1, datagram(json)));

  CHECK(param(*f.dev, "protocol/name") == ossia::value{std::string{"OpenTrackIO"}});
  CHECK(param(*f.dev, "protocol/version") == ossia::value{std::string{"1.0.1"}});

  CHECK(param(*f.dev, "sources/1/active") == ossia::value{true});
  CHECK(param(*f.dev, "sources/1/source_id")
        == ossia::value{std::string{"urn:uuid:9a4e9cb4-0fc3-4b29-bf35-621367beefa9"}});
  CHECK(param(*f.dev, "sources/1/sample_id")
        == ossia::value{std::string{"urn:uuid:b51513a1-7ffb-4291-b1b4-b369af6622b2"}});

  CHECK(param(*f.dev, "sources/1/tracker/status") == ossia::value{std::string{"Optical Good"}});
  CHECK(param(*f.dev, "sources/1/tracker/slate") == ossia::value{std::string{"A101_A_4"}});
  CHECK(param(*f.dev, "sources/1/tracker/recording") == ossia::value{false});

  CHECK(param(*f.dev, "sources/1/timing/mode") == ossia::value{std::string{"external"}});
  CHECK(approx_eq(param(*f.dev, "sources/1/timing/sample_rate_hz"), 24.f));
  CHECK(param(*f.dev, "sources/1/timing/timecode/hours") == ossia::value{1});
  CHECK(param(*f.dev, "sources/1/timing/timecode/minutes") == ossia::value{2});
  CHECK(param(*f.dev, "sources/1/timing/timecode/seconds") == ossia::value{3});
  CHECK(param(*f.dev, "sources/1/timing/timecode/frames") == ossia::value{4});

  CHECK(approx_eq(param(*f.dev, "sources/1/lens/f_stop"), 4.f));
  CHECK(approx_eq(param(*f.dev, "sources/1/lens/focus_distance_m"), 10.f));
  CHECK(approx_eq(param(*f.dev, "sources/1/lens/pinhole_focal_length_mm"), 24.305f));
  CHECK(approx_eq(param(*f.dev, "sources/1/lens/entrance_pupil_offset_m"), 0.123f));
  CHECK(approx_eq(param(*f.dev, "sources/1/lens/encoders/focus"), 0.1f));
  CHECK(approx_eq(param(*f.dev, "sources/1/lens/encoders/iris"), 0.2f));
  CHECK(approx_eq(param(*f.dev, "sources/1/lens/encoders/zoom"), 0.3f));
  CHECK(approx_eq(param(*f.dev, "sources/1/lens/projection_offset_mm/x"), 0.1f));
  CHECK(approx_eq(param(*f.dev, "sources/1/lens/projection_offset_mm/y"), 0.2f));
  CHECK(param(*f.dev, "sources/1/lens/distortion/0/radial")
        == ossia::value{std::vector<ossia::value>{1.f, 2.f, 3.f}});
  CHECK(param(*f.dev, "sources/1/lens/distortion/0/tangential")
        == ossia::value{std::vector<ossia::value>{1.f, 2.f}});
  CHECK(approx_eq(param(*f.dev, "sources/1/lens/distortion/0/overscan"), 3.1f));

  CHECK(param(*f.dev, "sources/1/transforms/Camera/translation")
        == ossia::value{ossia::vec3f{1.f, 2.f, 3.f}});
  CHECK(param(*f.dev, "sources/1/transforms/Camera/rotation_deg")
        == ossia::value{ossia::vec3f{180.f, 90.f, 45.f}});
  const auto rot = param(*f.dev, "sources/1/transforms/Camera/rotation").get<ossia::vec3f>();
  CHECK(std::abs(rot[0] - float(M_PI)) < 1e-4f);
  CHECK(std::abs(rot[1] - float(M_PI / 2)) < 1e-4f);
  CHECK(std::abs(rot[2] - float(M_PI / 4)) < 1e-4f);
}

TEST_CASE("OpenTrackIO: the complete static example fills camera, lens and timing", "[tracking][opentrackio]")
{
  Fixture f;
  const auto json = read_file("complete_static_example.json");
  REQUIRE(f.feed(1, datagram(json)));

  CHECK(param(*f.dev, "sources/1/camera/make") == ossia::value{std::string{"CameraMaker"}});
  CHECK(param(*f.dev, "sources/1/camera/model") == ossia::value{std::string{"Model20"}});
  CHECK(param(*f.dev, "sources/1/camera/serial") == ossia::value{std::string{"1234567890A"}});
  CHECK(param(*f.dev, "sources/1/camera/firmware") == ossia::value{std::string{"1.2.3"}});
  CHECK(param(*f.dev, "sources/1/camera/label") == ossia::value{std::string{"A"}});
  CHECK(param(*f.dev, "sources/1/camera/sensor_physical_mm") == ossia::value{ossia::vec2f{36.f, 24.f}});
  CHECK(param(*f.dev, "sources/1/camera/sensor_resolution_px") == ossia::value{ossia::vec2f{3840.f, 2160.f}});
  CHECK(approx_eq(param(*f.dev, "sources/1/camera/iso_speed"), 4000.f));
  CHECK(approx_eq(param(*f.dev, "sources/1/camera/shutter_angle"), 45.f));
  CHECK(approx_eq(param(*f.dev, "sources/1/camera/capture_frame_rate"), 24000.f / 1001.f));

  CHECK(param(*f.dev, "sources/1/lens/make") == ossia::value{std::string{"LensMaker"}});
  CHECK(param(*f.dev, "sources/1/lens/model") == ossia::value{std::string{"Model15"}});
  CHECK(approx_eq(param(*f.dev, "sources/1/lens/nominal_focal_length_mm"), 14.f));
  CHECK(approx_eq(param(*f.dev, "sources/1/lens/t_stop"), 4.1f));
  CHECK(param(*f.dev, "sources/1/lens/raw_encoders/focus") == ossia::value{1000});
  CHECK(param(*f.dev, "sources/1/lens/raw_encoders/zoom") == ossia::value{3000});
  CHECK(approx_eq(param(*f.dev, "sources/1/lens/distortion_offset_mm/x"), 1.f));
  CHECK(approx_eq(param(*f.dev, "sources/1/lens/exposure_falloff/a3"), 3.f));
  CHECK(param(*f.dev, "sources/1/lens/distortion/0/model") == ossia::value{std::string{"Brown-Conrady U-D"}});
  CHECK(param(*f.dev, "sources/1/lens/distortion/1/radial")
        == ossia::value{std::vector<ossia::value>{1.f, 2.f, 3.f, 4.f, 5.f, 6.f}});

  CHECK(param(*f.dev, "sources/1/tracker/make") == ossia::value{std::string{"TrackerMaker"}});
  CHECK(param(*f.dev, "sources/1/timing/mode") == ossia::value{std::string{"internal"}});
  CHECK(param(*f.dev, "sources/1/timing/sample_timestamp_s") == ossia::value{1718806554});
  CHECK(param(*f.dev, "sources/1/timing/sample_timestamp_ns") == ossia::value{500000000});
  CHECK(param(*f.dev, "sources/1/timing/timecode/drop_frame") == ossia::value{true});
  CHECK(approx_eq(param(*f.dev, "sources/1/timing/timecode/frame_rate"), 24000.f / 1001.f));
  CHECK(param(*f.dev, "sources/1/timing/sync/locked") == ossia::value{true});
  CHECK(param(*f.dev, "sources/1/timing/sync/source") == ossia::value{std::string{"ptp"}});
  CHECK(param(*f.dev, "sources/1/timing/sync/ptp/domain") == ossia::value{1});
  CHECK(param(*f.dev, "sources/1/timing/sync/ptp/leader_identity") == ossia::value{std::string{"00:11:22:33:44:55"}});

  CHECK(approx_eq(param(*f.dev, "duration_s"), 1.f / 25.f));
  CHECK(param(*f.dev, "related_samples")
        == ossia::value{std::vector<ossia::value>{
            std::string{"urn:uuid:2f2c8242-c6f5-4581-855f-8269243b6b43"},
            std::string{"urn:uuid:52be11ac-67e2-40a6-8ee1-1cc1a2b1c857"}}});
  CHECK(param(*f.dev, "global_stage/enu") == ossia::value{ossia::vec3f{100.f, 200.f, 300.f}});
  CHECK(approx_eq(param(*f.dev, "global_stage/lat0"), 100.f));

  CHECK(param(*f.dev, "sources/1/transforms/Dolly/translation") == ossia::value{ossia::vec3f{1.f, 2.f, 3.f}});
}

TEST_CASE("OpenTrackIO: every published example decodes", "[tracking][opentrackio]")
{
  for(const char* name :
      {"recommended_dynamic_example.json", "recommended_static_example.json",
       "complete_dynamic_example.json", "complete_static_example.json"})
  {
    INFO(name);
    Fixture f;
    CHECK(f.feed(1, datagram(read_file(name))));
    CHECK(param(*f.dev, "sources/1/active") == ossia::value{true});
  }
}

TEST_CASE("OpenTrackIO: a sample split over segments is reassembled", "[tracking][opentrackio]")
{
  Fixture f;
  const auto json = read_file("recommended_dynamic_example.json");
  const std::size_t cut = json.size() / 2;
  const std::string a = json.substr(0, cut), b = json.substr(cut);

  // First segment: nothing applied yet
  CHECK(!f.feed(1, datagram(a, OTRK_ENC_JSON, 10, 0, false)));
  CHECK(param(*f.dev, "sources/1/active") == ossia::value{false});
  // Last segment: the whole sample lands
  CHECK(f.feed(1, datagram(b, OTRK_ENC_JSON, 11, uint32_t(cut), true)));
  CHECK(param(*f.dev, "sources/1/active") == ossia::value{true});
  CHECK(param(*f.dev, "sources/1/transforms/Camera/translation") == ossia::value{ossia::vec3f{1.f, 2.f, 3.f}});

  // A segment with a gap is dropped until a fresh start
  Fixture g;
  CHECK(!g.feed(1, datagram(a, OTRK_ENC_JSON, 20, 0, false)));
  CHECK(!g.feed(1, datagram(b, OTRK_ENC_JSON, 21, uint32_t(cut) + 5, true)));
  CHECK(param(*g.dev, "sources/1/active") == ossia::value{false});
  CHECK(g.feed(1, datagram(json, OTRK_ENC_JSON, 22)));
  CHECK(param(*g.dev, "sources/1/active") == ossia::value{true});
}

TEST_CASE("OpenTrackIO: duplicates, corruption and wrong encodings are rejected", "[tracking][opentrackio]")
{
  Fixture f;
  const auto json = read_file("recommended_dynamic_example.json");
  auto d = datagram(json, OTRK_ENC_JSON, 5);
  REQUIRE(f.feed(1, d));
  // Same sequence number again on the same source: a retransmit
  CHECK(!f.feed(1, d));
  // Next sequence number: fine
  CHECK(f.feed(1, datagram(json, OTRK_ENC_JSON, 6)));

  // Corrupted payload: checksum mismatch
  auto bad = datagram(json, OTRK_ENC_JSON, 7);
  bad[OTRK_HEADER_LEN + 3] ^= 0x01;
  CHECK(!f.feed(1, bad));

  // JSON text announced as CBOR: undecodable
  CHECK(!f.feed(1, datagram(json, OTRK_ENC_CBOR, 8)));

  // Not an OTrk datagram at all
  CHECK(!f.proto->process_datagram(1, "hello", 5));
}
