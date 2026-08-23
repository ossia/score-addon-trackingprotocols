#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/**
 * RTTrPM (Real-Time Tracking Protocol - Motion) wire format parser.
 *
 * Free of Qt and ossia so that it can be tested on its own against the
 * protocol definition:
 *   https://rttrp.github.io/RTTrP-Wiki/RTTrPM.html
 *   https://github.com/RTTrP/RTTrP-v2.4.2.0 (reference decoders, vendored in
 *   3rdparty/RTTrP)
 *
 * Layout (all sizes in bytes). The RTTrP header is common to RTTrPM / RTTrPL:
 *
 *   Integer signature   u16   0x4154 big endian ("AT"), 0x5441 little endian
 *   Float signature     u16   0x4334 big endian, 0x3443 little endian (motion)
 *   Version             u16   0x0002
 *   Packet ID           u32   sequence number
 *   Packet format       u8    0x00 raw, 0x01 protobuf, 0x02 thrift
 *   Size                u16   of the whole packet, header included
 *   Context             u32   user-definable
 *   Number of modules   u8
 *
 * followed by that many packet modules. Every module starts with
 *   Type  u8
 *   Size  u16   "size of the packet module including the type and size"
 *
 * Trackable modules (0x01 without timestamp, 0x51 with) then carry
 *   Name length u8, Name, [Timestamp u32 (0x51 only)], Number of sub-modules u8
 * and their sub-modules:
 *   0x02 Centroid                latency u16, x y z f64
 *   0x03 Quaternion              latency u16, qx qy qz qw f64
 *   0x04 Euler                   latency u16, order u16, r1 r2 r3 f64 (radians)
 *   0x06 Tracked point (LED)     latency u16, x y z f64, index u8
 *   0x20 Centroid accel/vel      x y z f64, ax ay az f32, vx vy vz f32
 *   0x21 Tracked point accel/vel x y z f64, ax ay az f32, vx vy vz f32, index u8
 *   0x22 Zone collision          number of zones u8, then per zone:
 *                                size u8 (including itself), name length u8, name
 *
 * Multi-byte integers and floating point numbers follow the signature's byte
 * order, as in the reference decoders.
 */
namespace RTTrP
{
// RTTrP module type IDs
enum class ModuleType : uint8_t
{
  Trackable = 0x01,
  CentroidMod = 0x02,
  QuatModule = 0x03,
  EulerModule = 0x04,
  LEDModule = 0x06,
  CentroidAccVelMod = 0x20,
  LEDAccVelMod = 0x21,
  ZoneMod = 0x22,
  TrackableWithTimestamp = 0x51,
};

constexpr uint16_t RTTRP_INT_HEADER_BIG = 0x4154;
constexpr uint16_t RTTRP_INT_HEADER_LITTLE = 0x5441;
constexpr uint16_t RTTRPM_FLT_HEADER_BIG = 0x4334;
constexpr uint16_t RTTRPM_FLT_HEADER_LITTLE = 0x3443;
constexpr uint16_t RTTRP_VERSION = 0x0002;
constexpr std::size_t RTTRP_HEADER_SIZE = 18;
constexpr std::size_t RTTRP_MODULE_HEADER_SIZE = 3;

struct PacketHeader
{
  uint16_t int_signature{0};
  uint16_t float_signature{0};
  uint16_t version{0};
  uint32_t packet_id{0};
  uint8_t format{0};
  uint16_t size{0};
  uint32_t context{0};
  uint8_t num_modules{0};
  //! True when the signature says the payload is big endian.
  bool big_endian{false};
};

// Parsed trackable data
struct ParsedTrackable
{
  std::string name;
  bool has_timestamp{false};
  uint32_t timestamp{0};

  // Centroid position
  bool has_centroid{false};
  double x{}, y{}, z{};
  uint16_t latency{0};

  // Quaternion orientation
  bool has_quaternion{false};
  double qx{}, qy{}, qz{}, qw{};

  // Euler orientation, radians
  bool has_euler{false};
  double roll{}, pitch{}, yaw{};
  uint16_t euler_order{0};

  // Velocity and acceleration (from CentroidAccVelMod)
  bool has_velocity{false};
  float vx{}, vy{}, vz{};
  float ax{}, ay{}, az{};

  // LED markers
  struct LEDData
  {
    uint8_t index{0};
    double x{}, y{}, z{};
    float vx{}, vy{}, vz{};
    float ax{}, ay{}, az{};
    uint16_t latency{0};
    bool has_velocity{false};
  };
  std::vector<LEDData> leds;

  // Zones the trackable is colliding with
  std::vector<std::string> zones;
};

struct ParsedPacket
{
  PacketHeader header;
  std::vector<ParsedTrackable> trackables;
  //! Modules of a type this parser does not know about (counted, skipped).
  int unknown_modules{0};
};

//! Parses the 18-byte RTTrP header. Fails on a short buffer or an unknown
//! integer signature.
bool parse_header(const uint8_t* data, std::size_t size, PacketHeader& out);

//! Parses one trackable module, `module` pointing at its Type byte and
//! `module_size` being the module's Size field (which includes the 3-byte
//! module header).
bool parse_trackable(
    const uint8_t* module, std::size_t module_size, bool big_endian,
    ParsedTrackable& out);

//! Parses a whole RTTrPM packet. Returns false if the header is invalid; modules
//! that do not fit in the buffer are skipped.
bool parse_packet(const uint8_t* data, std::size_t size, ParsedPacket& out);
}
