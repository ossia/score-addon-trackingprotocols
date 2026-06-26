#include "OpenTrackIOProtocol.hpp"
#include "../Common/TrackingTreeBuilder.hpp"

#include <ossia/detail/logger.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/name_validation.hpp>
#include <ossia/network/base/node.hpp>

#include <opentrackio-cpp/OpenTrackIOSample.h>

#include <cstring>

namespace OpenTrackIO
{

// OpenTrackIO 1.0.1 wire format, per the SMPTE RIS-OSVP reference
// (ris-osvp-metadata-camdkit/src/test/python/parser).
//
//   Byte  Size  Field
//   0     4     "OTrk" (ASCII magic)
//   4     1     reserved (0)
//   5     1     encoding (0x01 = JSON, 0x02 = CBOR)
//   6     2     sequence_number (uint16 BE, per-segment, wraps at 0xFFFF)
//   8     4     segment_offset (uint32 BE, byte offset into reassembled payload)
//   12    2     [bit 15: last_segment] | [bits 0..14: payload_length]
//   14    2     Fletcher-16 checksum over header[0..14) ++ payload  (uint16 BE)
//   16    ...   payload (JSON text or CBOR bytes)
namespace
{
constexpr std::size_t OTRK_HEADER_LEN = 16;
constexpr uint8_t OTRK_ENC_JSON = 0x01;
constexpr uint8_t OTRK_ENC_CBOR = 0x02;

struct OTrkHeader
{
  uint8_t encoding{0};
  uint16_t sequence{0};
  uint32_t segment_offset{0};
  bool last_segment{false};
  uint16_t payload_length{0};
  uint16_t checksum{0};
};

inline uint16_t read_u16_be(const uint8_t* p) noexcept
{
  return uint16_t((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}
inline uint32_t read_u32_be(const uint8_t* p) noexcept
{
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
       | (uint32_t(p[2]) << 8)  |  uint32_t(p[3]);
}

uint16_t fletcher16(const uint8_t* data, std::size_t len) noexcept
{
  uint16_t sum1 = 0, sum2 = 0;
  for(std::size_t i = 0; i < len; ++i)
  {
    sum1 = uint16_t((sum1 + data[i]) % 256u);
    sum2 = uint16_t((sum2 + sum1) % 256u);
  }
  return uint16_t((sum2 << 8) | sum1);
}

bool parse_header(const char* data, std::size_t size, OTrkHeader& out)
{
  if(size < OTRK_HEADER_LEN)
    return false;
  const auto* d = reinterpret_cast<const uint8_t*>(data);
  if(std::memcmp(d, "OTrk", 4) != 0)
    return false;
  out.encoding = d[5];
  if(out.encoding != OTRK_ENC_JSON && out.encoding != OTRK_ENC_CBOR)
    return false;
  out.sequence       = read_u16_be(d + 6);
  out.segment_offset = read_u32_be(d + 8);
  const uint16_t len_flag = read_u16_be(d + 12);
  out.last_segment   = (len_flag & 0x8000) != 0;
  out.payload_length = uint16_t(len_flag & 0x7FFF);
  out.checksum       = read_u16_be(d + 14);
  if(size < OTRK_HEADER_LEN + out.payload_length)
    return false;
  // Fletcher-16 over header[0..14) ++ payload
  std::vector<uint8_t> buf;
  buf.reserve(14u + out.payload_length);
  buf.insert(buf.end(), d, d + 14);
  buf.insert(buf.end(), d + OTRK_HEADER_LEN, d + OTRK_HEADER_LEN + out.payload_length);
  return fletcher16(buf.data(), buf.size()) == out.checksum;
}
}

OpenTrackIOProtocol::OpenTrackIOProtocol(
    const ossia::net::network_context_ptr& ctx,
    const OpenTrackIOSpecificSettings& settings)
    : m_ctx{ctx}
    , m_settings{settings}
{
}

OpenTrackIOProtocol::~OpenTrackIOProtocol()
{
  stop_listeners();
}

void OpenTrackIOProtocol::set_device(ossia::net::device_base& dev)
{
  m_device = &dev;
  create_device_tree(dev.get_root_node());
  start_listeners();
}

// ----- Device tree ---------------------------------------------------------

void OpenTrackIOProtocol::create_source_slot(
    ossia::net::node_base& sourcesNode, int sourceNumber)
{
  using TB = Tracking::TreeBuilder;
  auto* slot = sourcesNode.create_child(std::to_string(sourceNumber));
  TB::create_bool_param(*slot, "active", false);
  TB::create_string_param(*slot, "source_id", "");
  TB::create_string_param(*slot, "sample_id", "");
  TB::create_int_param(*slot, "sequence", 0);

  if(m_settings.enableCamera)
  {
    auto* cam = slot->create_child("camera");
    TB::create_string_param(*cam, "make", "");
    TB::create_string_param(*cam, "model", "");
    TB::create_string_param(*cam, "serial", "");
    TB::create_string_param(*cam, "firmware", "");
    TB::create_string_param(*cam, "label", "");
    TB::create_vec2f_param(*cam, "sensor_physical_mm", {0.f, 0.f}, 0.f, 1000.f);
    TB::create_vec2f_param(*cam, "sensor_resolution_px", {0.f, 0.f}, 0.f, 1e5f);
    TB::create_float_param(*cam, "anamorphic_squeeze", 1.f, 0.1f, 10.f);
    TB::create_float_param(*cam, "iso_speed", 0.f, 0.f, 1e6f);
    TB::create_float_param(*cam, "shutter_angle", 180.f, 0.f, 360.f);
    TB::create_float_param(*cam, "capture_frame_rate", 0.f, 0.f, 1000.f);
  }

  {
    auto* tr = slot->create_child("tracker");
    TB::create_string_param(*tr, "make", "");
    TB::create_string_param(*tr, "model", "");
    TB::create_string_param(*tr, "serial", "");
    TB::create_string_param(*tr, "firmware", "");
    TB::create_string_param(*tr, "notes", "");
    TB::create_string_param(*tr, "slate", "");
    TB::create_string_param(*tr, "status", "");
    TB::create_bool_param(*tr, "recording", false);
  }

  {
    auto* transforms = slot->create_child("transforms");
    auto* main_tx = transforms->create_child("Camera");
    TB::create_vec3f_param(*main_tx, "translation");
    TB::create_vec3f_param(*main_tx, "rotation", {0.f, 0.f, 0.f}, -(float)M_PI, (float)M_PI);
    TB::create_vec3f_param(*main_tx, "rotation_deg", {0.f, 0.f, 0.f}, -360.f, 360.f);
    TB::create_vec3f_param(*main_tx, "scale", {1.f, 1.f, 1.f}, 0.f, 100.f);
  }

  if(m_settings.enableLens)
  {
    auto* lens = slot->create_child("lens");
    TB::create_string_param(*lens, "make", "");
    TB::create_string_param(*lens, "model", "");
    TB::create_string_param(*lens, "serial", "");
    TB::create_string_param(*lens, "firmware", "");
    TB::create_float_param(*lens, "nominal_focal_length_mm", 0.f, 0.f, 10000.f);
    TB::create_list_param(*lens, "custom");              // free-form coefficients
    TB::create_list_param(*lens, "calibration_history"); // list of history strings
    TB::create_float_param(*lens, "pinhole_focal_length_mm", 0.f, 0.f, 10000.f);
    TB::create_float_param(*lens, "focus_distance_m", 0.f, 0.f, 1e6f);
    TB::create_float_param(*lens, "f_stop", 0.f, 0.f, 64.f);
    TB::create_float_param(*lens, "t_stop", 0.f, 0.f, 64.f);
    TB::create_float_param(*lens, "entrance_pupil_offset_m", 0.f, -10.f, 10.f);

    auto* enc = lens->create_child("encoders");
    TB::create_float_param(*enc, "focus", 0.f, 0.f, 1.f);
    TB::create_float_param(*enc, "iris", 0.f, 0.f, 1.f);
    TB::create_float_param(*enc, "zoom", 0.f, 0.f, 1.f);

    auto* raw = lens->create_child("raw_encoders");
    TB::create_int_param(*raw, "focus", 0);
    TB::create_int_param(*raw, "iris", 0);
    TB::create_int_param(*raw, "zoom", 0);

    auto* dist = lens->create_child("distortion");
    TB::create_float_param(*dist, "overscan_max", 1.f, 0.f, 100.f);
    TB::create_float_param(*dist, "undistort_overscan_max", 1.f, 0.f, 100.f);

    auto* doff = lens->create_child("distortion_offset_mm");
    TB::create_float_param(*doff, "x", 0.f, -1000.f, 1000.f);
    TB::create_float_param(*doff, "y", 0.f, -1000.f, 1000.f);

    auto* poff = lens->create_child("projection_offset_mm");
    TB::create_float_param(*poff, "x", 0.f, -1000.f, 1000.f);
    TB::create_float_param(*poff, "y", 0.f, -1000.f, 1000.f);

    auto* exp = lens->create_child("exposure_falloff");
    TB::create_float_param(*exp, "a1", 0.f, -10.f, 10.f);
    TB::create_float_param(*exp, "a2", 0.f, -10.f, 10.f);
    TB::create_float_param(*exp, "a3", 0.f, -10.f, 10.f);
  }

  if(m_settings.enableTiming)
  {
    auto* tm = slot->create_child("timing");
    TB::create_string_param(*tm, "mode", "");
    TB::create_int_param(*tm, "sequence_number", 0);
    TB::create_float_param(*tm, "sample_rate_hz", 0.f, 0.f, 1000.f);
    TB::create_int_param(*tm, "sample_timestamp_s", 0);
    TB::create_int_param(*tm, "sample_timestamp_ns", 0);
    TB::create_int_param(*tm, "recorded_timestamp_s", 0);
    TB::create_int_param(*tm, "recorded_timestamp_ns", 0);

    auto* tc = tm->create_child("timecode");
    TB::create_int_param(*tc, "hours", 0);
    TB::create_int_param(*tc, "minutes", 0);
    TB::create_int_param(*tc, "seconds", 0);
    TB::create_int_param(*tc, "frames", 0);
    TB::create_float_param(*tc, "frame_rate", 0.f, 0.f, 240.f);
    TB::create_bool_param(*tc, "drop_frame", false);

    auto* sync = tm->create_child("sync");
    TB::create_bool_param(*sync, "locked", false);
    TB::create_bool_param(*sync, "present", false);
    TB::create_string_param(*sync, "source", "");
    TB::create_float_param(*sync, "frequency_hz", 0.f, 0.f, 1000.f);

    auto* off = sync->create_child("offsets");
    TB::create_float_param(*off, "translation", 0.f, -10.f, 10.f);
    TB::create_float_param(*off, "rotation", 0.f, -10.f, 10.f);
    TB::create_float_param(*off, "lens_encoders", 0.f, -10.f, 10.f);

    auto* ptp = sync->create_child("ptp");
    TB::create_string_param(*ptp, "profile", "");
    TB::create_int_param(*ptp, "domain", 0);
    TB::create_string_param(*ptp, "leader_identity", "");
    TB::create_int_param(*ptp, "priority1", 0);
    TB::create_int_param(*ptp, "priority2", 0);
    TB::create_float_param(*ptp, "leader_accuracy_s", 0.f, -1.f, 1.f);
    TB::create_float_param(*ptp, "mean_path_delay_s", 0.f, 0.f, 1.f);
    TB::create_int_param(*ptp, "vlan", 0);
    TB::create_string_param(*ptp, "leader_time_source", "");
  }
}

void OpenTrackIOProtocol::create_device_tree(ossia::net::node_base& root)
{
  using TB = Tracking::TreeBuilder;

  auto* proto = root.create_child("protocol");
  TB::create_string_param(*proto, "name", "");
  TB::create_string_param(*proto, "version", "");

  // Duration of the current clip (from static.duration in the sample).
  TB::create_float_param(root, "duration_s", 0.f, 0.f, 1e9f);

  // Related sample IDs: LIST of UUID strings linking this sample to others.
  TB::create_list_param(root, "related_samples");

  if(m_settings.enableGlobalStage)
  {
    auto* gs = root.create_child("global_stage");
    TB::create_vec3f_param(*gs, "enu", {0.f, 0.f, 0.f}, -1e7f, 1e7f);
    TB::create_float_param(*gs, "lat0", 0.f, -90.f, 90.f);
    TB::create_float_param(*gs, "lon0", 0.f, -180.f, 180.f);
    TB::create_float_param(*gs, "h0", 0.f, -1e4f, 1e4f);
  }

  auto* sources = root.create_child("sources");
  const int lo = m_settings.minSourceNumber;
  const int hi = std::max(lo, int(m_settings.maxSourceNumber));
  for(int n = lo; n <= hi; ++n)
    create_source_slot(*sources, n);
}

ossia::net::node_base*
OpenTrackIOProtocol::get_or_create_source_slot(int sourceNumber)
{
  auto& root = m_device->get_root_node();
  auto* sources = root.find_child(std::string_view("sources"));
  if(!sources)
    return nullptr;
  auto key = std::to_string(sourceNumber);
  if(auto* existing = sources->find_child(key))
    return existing;
  // Sample arrived for a source outside the configured range — build a full
  // slot (camera/tracker/lens/transforms/timing children) so the subsequent
  // updates find their parameters.
  create_source_slot(*sources, sourceNumber);
  return sources->find_child(key);
}

// ----- Listeners (one UDP socket per source) -------------------------------

void OpenTrackIOProtocol::start_listeners()
{
  const int lo = m_settings.minSourceNumber;
  const int hi = std::max(lo, int(m_settings.maxSourceNumber));

  for(int n = lo; n <= hi; ++n)
  {
    auto listener = std::make_unique<SourceListener>();
    listener->source_number = n;

    try
    {
      const std::string group
          = m_settings.multicastBaseAddress.toStdString() + "." + std::to_string(n);
      ossia::net::inbound_socket_configuration config{
          .bind = "0.0.0.0",
          .port = m_settings.port,
          .multicast_group = group,
      };
      listener->socket = std::make_unique<ossia::net::udp_receive_socket>(
          config, m_ctx->context);
      listener->socket->open();

      auto* raw = listener.get();
      listener->socket->receive(
          [this, raw](const char* data, std::size_t sz) { on_datagram(*raw, data, sz); });
    }
    catch(const std::exception& e)
    {
      ossia::logger().error(
          "OpenTrackIO: source {} socket setup failed: {}", n, e.what());
      continue;
    }
    m_listeners.push_back(std::move(listener));
  }
}

void OpenTrackIOProtocol::stop_listeners()
{
  for(auto& l : m_listeners)
  {
    if(l && l->socket)
      l->socket->close();
  }
  m_listeners.clear();
}

void OpenTrackIOProtocol::on_datagram(
    SourceListener& s, const char* data, std::size_t size)
{
  if(!m_device)
    return;

  OTrkHeader hdr{};
  if(!parse_header(data, size, hdr))
    return;

  // Duplicate packet (same sequence as previous on this source).
  if(s.have_prev_sequence && hdr.sequence == s.prev_sequence)
    return;
  s.prev_sequence = hdr.sequence;
  s.have_prev_sequence = true;
  s.current_encoding = hdr.encoding;

  // Segments must arrive in offset order; any gap resyncs on offset==0.
  if(hdr.segment_offset != s.reassembly.size())
  {
    s.reassembly.clear();
    if(hdr.segment_offset != 0)
      return;
  }
  const auto* payload
      = reinterpret_cast<const uint8_t*>(data) + OTRK_HEADER_LEN;
  s.reassembly.insert(
      s.reassembly.end(), payload, payload + hdr.payload_length);

  if(!hdr.last_segment)
    return;

  opentrackio::OpenTrackIOSample sample;
  bool ok = false;
  try
  {
    if(s.current_encoding == OTRK_ENC_JSON && m_settings.acceptJSON)
      ok = sample.initialise(std::string_view{
          reinterpret_cast<const char*>(s.reassembly.data()), s.reassembly.size()});
    else if(s.current_encoding == OTRK_ENC_CBOR && m_settings.acceptCBOR)
      ok = sample.initialise(std::span<const uint8_t>{
          s.reassembly.data(), s.reassembly.size()});
  }
  catch(const std::exception& e)
  {
    ossia::logger().warn(
        "OpenTrackIO: source {} decode exception: {}", s.source_number, e.what());
    s.reassembly.clear();
    return;
  }
  s.reassembly.clear();

  if(!ok)
  {
    for(const auto& msg : sample.getErrors())
      ossia::logger().debug(
          "OpenTrackIO source {}: parse error: {}", s.source_number, msg);
    return;
  }

  // The sample carries its own sourceNumber. Prefer that; fall back to the
  // listener's configured number if the sample omits the field.
  const int reported_src
      = sample.sourceNumber ? int(sample.sourceNumber->value) : s.source_number;
  apply_sample(sample, reported_src);
}

// ----- Decode + apply ------------------------------------------------------

namespace
{
using TB = Tracking::TreeBuilder;
using opentrackio::OpenTrackIOSample;
namespace otp = opentrackio::opentrackioproperties;

void apply_protocol(const OpenTrackIOSample& s, ossia::net::node_base& root)
{
  if(!s.protocol)
    return;
  if(auto* p = root.find_child(std::string_view("protocol")))
  {
    TB::update_param(p, "name", s.protocol->name);
    std::string v;
    for(auto n : s.protocol->version)
    {
      if(!v.empty())
        v += '.';
      v += std::to_string(n);
    }
    TB::update_param(p, "version", v);
  }
}

void apply_duration(const OpenTrackIOSample& s, ossia::net::node_base& root)
{
  if(!s.duration)
    return;
  const auto& r = s.duration->rational;
  if(r.denominator == 0)
    return;
  const float secs = float(double(r.numerator) / double(r.denominator));
  TB::update_param(&root, "duration_s", secs);
}

void apply_related_samples(const OpenTrackIOSample& s, ossia::net::node_base& root)
{
  if(!s.relatedSampleIds)
    return;
  std::vector<ossia::value> out;
  out.reserve(s.relatedSampleIds->samples.size());
  for(const auto& id : s.relatedSampleIds->samples)
    out.emplace_back(id);
  TB::update_param(&root, "related_samples", std::move(out));
}

void apply_global_stage(const OpenTrackIOSample& s, ossia::net::node_base& root)
{
  if(!s.globalStage)
    return;
  auto* gs = root.find_child(std::string_view("global_stage"));
  if(!gs)
    return;
  TB::update_param(
      gs, "enu",
      ossia::vec3f{float(s.globalStage->e), float(s.globalStage->n), float(s.globalStage->u)});
  TB::update_param(gs, "lat0", float(s.globalStage->lat0));
  TB::update_param(gs, "lon0", float(s.globalStage->lon0));
  TB::update_param(gs, "h0", float(s.globalStage->h0));
}

void apply_camera(const otp::Camera& c, ossia::net::node_base& camNode)
{
  if(c.make)           TB::update_param(&camNode, "make", *c.make);
  if(c.model)          TB::update_param(&camNode, "model", *c.model);
  if(c.serialNumber)   TB::update_param(&camNode, "serial", *c.serialNumber);
  if(c.firmwareVersion)TB::update_param(&camNode, "firmware", *c.firmwareVersion);
  if(c.label)          TB::update_param(&camNode, "label", *c.label);
  if(c.activeSensorPhysicalDimensions)
    TB::update_param(
        &camNode, "sensor_physical_mm",
        ossia::vec2f{float(c.activeSensorPhysicalDimensions->width),
                     float(c.activeSensorPhysicalDimensions->height)});
  if(c.activeSensorResolution)
    TB::update_param(
        &camNode, "sensor_resolution_px",
        ossia::vec2f{float(c.activeSensorResolution->width),
                     float(c.activeSensorResolution->height)});
  if(c.anamorphicSqueeze && c.anamorphicSqueeze->denominator != 0)
    TB::update_param(
        &camNode, "anamorphic_squeeze",
        float(double(c.anamorphicSqueeze->numerator) / double(c.anamorphicSqueeze->denominator)));
  if(c.isoSpeed) TB::update_param(&camNode, "iso_speed", float(*c.isoSpeed));
  if(c.shutterAngle) TB::update_param(&camNode, "shutter_angle", float(*c.shutterAngle));
  if(c.captureFrameRate && c.captureFrameRate->denominator != 0)
    TB::update_param(
        &camNode, "capture_frame_rate",
        float(double(c.captureFrameRate->numerator) / double(c.captureFrameRate->denominator)));
}

void apply_tracker(const otp::Tracker& t, ossia::net::node_base& trNode)
{
  if(t.make)            TB::update_param(&trNode, "make", *t.make);
  if(t.model)           TB::update_param(&trNode, "model", *t.model);
  if(t.serialNumber)    TB::update_param(&trNode, "serial", *t.serialNumber);
  if(t.firmwareVersion) TB::update_param(&trNode, "firmware", *t.firmwareVersion);
  if(t.notes)           TB::update_param(&trNode, "notes", *t.notes);
  if(t.slate)           TB::update_param(&trNode, "slate", *t.slate);
  if(t.status)          TB::update_param(&trNode, "status", *t.status);
  if(t.recording)       TB::update_param(&trNode, "recording", *t.recording);
}

void apply_transforms(const otp::Transforms& tf, ossia::net::node_base& slot)
{
  auto* transforms = slot.find_child(std::string_view("transforms"));
  if(!transforms)
    transforms = slot.create_child("transforms");

  constexpr float d2r = static_cast<float>(M_PI / 180.0);
  for(const auto& t : tf.transforms)
  {
    // The transform id comes straight off the wire; sanitize it so that
    // spaces / slashes / non-ASCII don't produce invalid OSC paths.
    std::string id = ossia::net::sanitize_name(t.id ? *t.id : std::string{"Camera"});
    if(id.empty())
      id = "Camera";
    auto* tx = transforms->find_child(id);
    if(!tx)
    {
      tx = transforms->create_child(id);
      TB::create_vec3f_param(*tx, "translation");
      TB::create_vec3f_param(*tx, "rotation", {0.f, 0.f, 0.f}, -(float)M_PI, (float)M_PI);
      TB::create_vec3f_param(*tx, "rotation_deg", {0.f, 0.f, 0.f}, -360.f, 360.f);
      TB::create_vec3f_param(*tx, "scale", {1.f, 1.f, 1.f}, 0.f, 100.f);
    }
    TB::update_param(
        tx, "translation",
        ossia::vec3f{float(t.translation.x), float(t.translation.y), float(t.translation.z)});
    ossia::vec3f rot_deg{float(t.rotation.pan), float(t.rotation.tilt), float(t.rotation.roll)};
    TB::update_param(tx, "rotation_deg", rot_deg);
    TB::update_param(
        tx, "rotation",
        ossia::vec3f{rot_deg[0] * d2r, rot_deg[1] * d2r, rot_deg[2] * d2r});
    if(t.scale)
      TB::update_param(
          tx, "scale", ossia::vec3f{float(t.scale->x), float(t.scale->y), float(t.scale->z)});
  }
}

void apply_lens(const otp::Lens& l, ossia::net::node_base& lensNode)
{
  if(l.make)               TB::update_param(&lensNode, "make", *l.make);
  if(l.model)              TB::update_param(&lensNode, "model", *l.model);
  if(l.serialNumber)       TB::update_param(&lensNode, "serial", *l.serialNumber);
  if(l.firmwareVersion)    TB::update_param(&lensNode, "firmware", *l.firmwareVersion);
  if(l.nominalFocalLength) TB::update_param(&lensNode, "nominal_focal_length_mm", float(*l.nominalFocalLength));

  if(l.custom)
  {
    std::vector<ossia::value> out;
    out.reserve(l.custom->size());
    for(double v : *l.custom)
      out.emplace_back(float(v));
    TB::update_param(&lensNode, "custom", std::move(out));
  }
  if(l.calibrationHistory)
  {
    std::vector<ossia::value> out;
    out.reserve(l.calibrationHistory->size());
    for(const auto& h : *l.calibrationHistory)
      out.emplace_back(h);
    TB::update_param(&lensNode, "calibration_history", std::move(out));
  }

  if(l.pinholeFocalLength) TB::update_param(&lensNode, "pinhole_focal_length_mm", float(*l.pinholeFocalLength));
  if(l.focusDistance)      TB::update_param(&lensNode, "focus_distance_m", float(*l.focusDistance));
  if(l.fStop)              TB::update_param(&lensNode, "f_stop", float(*l.fStop));
  if(l.tStop)              TB::update_param(&lensNode, "t_stop", float(*l.tStop));
  if(l.entrancePupilOffset)
    TB::update_param(&lensNode, "entrance_pupil_offset_m", float(*l.entrancePupilOffset));

  if(l.encoders)
  {
    auto* enc = lensNode.find_child(std::string_view("encoders"));
    if(enc)
    {
      if(l.encoders->focus) TB::update_param(enc, "focus", float(*l.encoders->focus));
      if(l.encoders->iris)  TB::update_param(enc, "iris",  float(*l.encoders->iris));
      if(l.encoders->zoom)  TB::update_param(enc, "zoom",  float(*l.encoders->zoom));
    }
  }
  if(l.rawEncoders)
  {
    auto* raw = lensNode.find_child(std::string_view("raw_encoders"));
    if(raw)
    {
      if(l.rawEncoders->focus) TB::update_param(raw, "focus", int(*l.rawEncoders->focus));
      if(l.rawEncoders->iris)  TB::update_param(raw, "iris",  int(*l.rawEncoders->iris));
      if(l.rawEncoders->zoom)  TB::update_param(raw, "zoom",  int(*l.rawEncoders->zoom));
    }
  }

  auto* distNode = lensNode.find_child(std::string_view("distortion"));
  if(distNode && l.distortionOverscanMax)
    TB::update_param(distNode, "overscan_max", float(*l.distortionOverscanMax));
  if(distNode && l.undistortionOverscanMax)
    TB::update_param(distNode, "undistort_overscan_max", float(*l.undistortionOverscanMax));

  if(distNode && l.distortion)
  {
    auto double_vec_to_list = [](const std::vector<double>& in) {
      std::vector<ossia::value> out;
      out.reserve(in.size());
      for(double v : in)
        out.emplace_back(float(v));
      return out;
    };

    for(std::size_t i = 0; i < l.distortion->size(); ++i)
    {
      const auto& d = (*l.distortion)[i];
      auto key = std::to_string(i);
      auto* slot = distNode->find_child(key);
      if(!slot)
      {
        slot = distNode->create_child(key);
        TB::create_list_param(*slot, "radial");
        TB::create_list_param(*slot, "tangential");
        TB::create_string_param(*slot, "model", "");
        TB::create_float_param(*slot, "overscan", 1.f, 0.f, 100.f);
      }
      TB::update_param(slot, "radial", double_vec_to_list(d.radial));
      if(d.tangential)
        TB::update_param(slot, "tangential", double_vec_to_list(*d.tangential));
      if(d.model)
        TB::update_param(slot, "model", *d.model);
      if(d.overscan)
        TB::update_param(slot, "overscan", float(*d.overscan));
    }
  }

  if(l.distortionOffset)
  {
    auto* doff = lensNode.find_child(std::string_view("distortion_offset_mm"));
    if(doff)
    {
      TB::update_param(doff, "x", float(l.distortionOffset->x));
      TB::update_param(doff, "y", float(l.distortionOffset->y));
    }
  }
  if(l.projectionOffset)
  {
    auto* poff = lensNode.find_child(std::string_view("projection_offset_mm"));
    if(poff)
    {
      TB::update_param(poff, "x", float(l.projectionOffset->x));
      TB::update_param(poff, "y", float(l.projectionOffset->y));
    }
  }
  if(l.exposureFalloff)
  {
    auto* exp = lensNode.find_child(std::string_view("exposure_falloff"));
    if(exp)
    {
      TB::update_param(exp, "a1", float(l.exposureFalloff->a1));
      if(l.exposureFalloff->a2) TB::update_param(exp, "a2", float(*l.exposureFalloff->a2));
      if(l.exposureFalloff->a3) TB::update_param(exp, "a3", float(*l.exposureFalloff->a3));
    }
  }
}

void apply_timing(const otp::Timing& t, ossia::net::node_base& tm)
{
  if(t.mode)
    TB::update_param(
        &tm, "mode",
        *t.mode == otp::Timing::Mode::INTERNAL ? std::string{"internal"} : std::string{"external"});
  if(t.sequenceNumber)
    TB::update_param(&tm, "sequence_number", int(*t.sequenceNumber));
  if(t.sampleRate && t.sampleRate->denominator != 0)
    TB::update_param(
        &tm, "sample_rate_hz",
        float(double(t.sampleRate->numerator) / double(t.sampleRate->denominator)));
  if(t.sampleTimestamp)
  {
    TB::update_param(&tm, "sample_timestamp_s", int(t.sampleTimestamp->seconds));
    TB::update_param(&tm, "sample_timestamp_ns", int(t.sampleTimestamp->nanoseconds));
  }
  if(t.recordedTimestamp)
  {
    TB::update_param(&tm, "recorded_timestamp_s", int(t.recordedTimestamp->seconds));
    TB::update_param(&tm, "recorded_timestamp_ns", int(t.recordedTimestamp->nanoseconds));
  }
  if(t.timecode)
  {
    if(auto* tc = tm.find_child(std::string_view("timecode")))
    {
      TB::update_param(tc, "hours",   int(t.timecode->hours));
      TB::update_param(tc, "minutes", int(t.timecode->minutes));
      TB::update_param(tc, "seconds", int(t.timecode->seconds));
      TB::update_param(tc, "frames",  int(t.timecode->frames));
      if(t.timecode->frameRate.denominator != 0)
        TB::update_param(
            tc, "frame_rate",
            float(double(t.timecode->frameRate.numerator)
                  / double(t.timecode->frameRate.denominator)));
      if(t.timecode->dropFrame)
        TB::update_param(tc, "drop_frame", *t.timecode->dropFrame);
    }
  }
  if(t.synchronization)
  {
    auto* sync = tm.find_child(std::string_view("sync"));
    if(!sync)
      return;
    const auto& s = *t.synchronization;
    TB::update_param(sync, "locked", s.locked);
    if(s.present)
      TB::update_param(sync, "present", *s.present);
    std::string src_str;
    using ST = otp::Timing::Synchronization::SourceType;
    switch(s.source)
    {
      case ST::GEN_LOCK: src_str = "genlock"; break;
      case ST::VIDEO_IN: src_str = "videoIn"; break;
      case ST::PTP:      src_str = "ptp"; break;
      case ST::NTP:      src_str = "ntp"; break;
    }
    TB::update_param(sync, "source", src_str);
    if(s.frequency && s.frequency->denominator != 0)
      TB::update_param(
          sync, "frequency_hz",
          float(double(s.frequency->numerator) / double(s.frequency->denominator)));

    if(s.offsets)
    {
      if(auto* off = sync->find_child(std::string_view("offsets")))
      {
        if(s.offsets->translation)
          TB::update_param(off, "translation", float(*s.offsets->translation));
        if(s.offsets->rotation)
          TB::update_param(off, "rotation", float(*s.offsets->rotation));
        if(s.offsets->lensEncoders)
          TB::update_param(off, "lens_encoders", float(*s.offsets->lensEncoders));
      }
    }
    if(s.ptp)
    {
      if(auto* ptp = sync->find_child(std::string_view("ptp")))
      {
        using PP = otp::Timing::Synchronization::Ptp::ProfileType;
        using LT = otp::Timing::Synchronization::Ptp::LeaderTimeSourceType;
        std::string profile;
        switch(s.ptp->profile)
        {
          case PP::IEEE_Std_1588_2019:    profile = "IEEE Std 1588-2019"; break;
          case PP::IEEE_Std_802_1AS_2020: profile = "IEEE Std 802.1AS-2020"; break;
          case PP::SMPTE_ST2059_2_2021:   profile = "SMPTE ST2059-2:2021"; break;
        }
        TB::update_param(ptp, "profile", profile);
        TB::update_param(ptp, "domain", int(s.ptp->domain));
        TB::update_param(ptp, "leader_identity", s.ptp->leaderIdentity);
        TB::update_param(ptp, "priority1", int(s.ptp->leaderPriorities.priority1));
        TB::update_param(ptp, "priority2", int(s.ptp->leaderPriorities.priority2));
        TB::update_param(ptp, "leader_accuracy_s", float(s.ptp->leaderAccuracy));
        TB::update_param(ptp, "mean_path_delay_s", float(s.ptp->meanPathDelay));
        if(s.ptp->vlan)
          TB::update_param(ptp, "vlan", int(*s.ptp->vlan));
        if(s.ptp->leaderTimeSource)
        {
          std::string lts;
          switch(*s.ptp->leaderTimeSource)
          {
            case LT::GNSS:         lts = "GNSS"; break;
            case LT::Atomic_clock: lts = "Atomic clock"; break;
            case LT::NTP:          lts = "NTP"; break;
          }
          TB::update_param(ptp, "leader_time_source", lts);
        }
      }
    }
  }
}
}

void OpenTrackIOProtocol::apply_sample(
    const opentrackio::OpenTrackIOSample& s, int sourceNumber)
{
  auto& root = m_device->get_root_node();

  apply_protocol(s, root);
  apply_global_stage(s, root);
  apply_duration(s, root);
  apply_related_samples(s, root);

  auto* slot = get_or_create_source_slot(sourceNumber);
  if(!slot)
    return;

  TB::update_param(slot, "active", true);
  if(s.sampleId)
    TB::update_param(slot, "sample_id", s.sampleId->id);
  if(s.sourceId)
    TB::update_param(slot, "source_id", s.sourceId->id);

  if(s.camera && m_settings.enableCamera)
  {
    if(auto* cam = slot->find_child(std::string_view("camera")))
      apply_camera(*s.camera, *cam);
  }
  if(s.tracker)
  {
    if(auto* tr = slot->find_child(std::string_view("tracker")))
      apply_tracker(*s.tracker, *tr);
  }
  if(s.transforms)
  {
    apply_transforms(*s.transforms, *slot);
  }
  if(s.lens && m_settings.enableLens)
  {
    if(auto* lens = slot->find_child(std::string_view("lens")))
      apply_lens(*s.lens, *lens);
  }
  if(s.timing && m_settings.enableTiming)
  {
    if(auto* tm = slot->find_child(std::string_view("timing")))
    {
      apply_timing(*s.timing, *tm);
      if(s.timing->sequenceNumber)
        TB::update_param(slot, "sequence", int(*s.timing->sequenceNumber));
    }
  }
}

}
