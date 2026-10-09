#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

/**
 * OpenTrackIO transport ("OTrk") datagram header, per OpenTrackIO 1.0.1:
 * https://ris-pub.smpte.org/ris-osvp-metadata-camdkit/ (section "Transport")
 *
 *   Byte  Size  Field
 *   0     4     "OTrk" (ASCII magic)
 *   4     1     reserved (0), to be ignored
 *   5     1     encoding (0x01 = JSON, 0x02 = CBOR; 0x80+ vendor specific)
 *   6     2     sequence number (uint16 BE, per segment, wraps at 0xFFFF)
 *   8     4     segment offset (uint32 BE, byte offset into the reassembled payload)
 *   12    2     [bit 15: last segment] | [bits 0..14: payload length]
 *   14    2     Fletcher-16 checksum (modulus 256) over header[0..14) ++ payload, BE
 *   16    ...   payload (JSON text or CBOR bytes)
 *
 * Free of Qt and ossia so that it can be tested on its own.
 */
namespace OpenTrackIO
{
constexpr std::size_t OTRK_HEADER_LEN = 16;
constexpr std::size_t OTRK_CHECKSUMMED_HEADER_LEN = 14;
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

inline uint16_t otrk_read_u16_be(const uint8_t* p) noexcept
{
  return uint16_t((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}
inline uint32_t otrk_read_u32_be(const uint8_t* p) noexcept
{
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8)
         | uint32_t(p[3]);
}
inline void otrk_write_u16_be(uint8_t* p, uint16_t v) noexcept
{
  p[0] = uint8_t(v >> 8);
  p[1] = uint8_t(v & 0xFF);
}
inline void otrk_write_u32_be(uint8_t* p, uint32_t v) noexcept
{
  p[0] = uint8_t(v >> 24);
  p[1] = uint8_t((v >> 16) & 0xFF);
  p[2] = uint8_t((v >> 8) & 0xFF);
  p[3] = uint8_t(v & 0xFF);
}

//! Fletcher-16 with modulus 256, as given in the OpenTrackIO specification.
inline uint16_t fletcher16(const uint8_t* data, std::size_t len) noexcept
{
  if(!data)
    return 0;
  uint8_t sum1 = 0, sum2 = 0;
  while(len-- > 0)
  {
    sum1 = uint8_t(sum1 + *data++);
    sum2 = uint8_t(sum2 + sum1);
  }
  return uint16_t((uint16_t(sum2) << 8) | uint16_t(sum1));
}

//! Fletcher-16 over two consecutive ranges, without concatenating them.
inline uint16_t
fletcher16(const uint8_t* a, std::size_t len_a, const uint8_t* b, std::size_t len_b) noexcept
{
  uint8_t sum1 = 0, sum2 = 0;
  for(std::size_t i = 0; i < len_a; i++)
  {
    sum1 = uint8_t(sum1 + a[i]);
    sum2 = uint8_t(sum2 + sum1);
  }
  for(std::size_t i = 0; i < len_b; i++)
  {
    sum1 = uint8_t(sum1 + b[i]);
    sum2 = uint8_t(sum2 + sum1);
  }
  return uint16_t((uint16_t(sum2) << 8) | uint16_t(sum1));
}

/**
 * @brief Parses and validates an OTrk datagram header.
 *
 * Fails on a short buffer, a bad magic, an unknown encoding, a payload longer
 * than the datagram, or a checksum mismatch.
 */
inline bool otrk_parse_header(const char* data, std::size_t size, OTrkHeader& out) noexcept
{
  if(!data || size < OTRK_HEADER_LEN)
    return false;
  const auto* d = reinterpret_cast<const uint8_t*>(data);
  if(std::memcmp(d, "OTrk", 4) != 0)
    return false;
  out.encoding = d[5];
  if(out.encoding != OTRK_ENC_JSON && out.encoding != OTRK_ENC_CBOR)
    return false;
  out.sequence = otrk_read_u16_be(d + 6);
  out.segment_offset = otrk_read_u32_be(d + 8);
  const uint16_t len_flag = otrk_read_u16_be(d + 12);
  out.last_segment = (len_flag & 0x8000) != 0;
  out.payload_length = uint16_t(len_flag & 0x7FFF);
  out.checksum = otrk_read_u16_be(d + 14);
  if(size < OTRK_HEADER_LEN + out.payload_length)
    return false;

  return fletcher16(
             d, OTRK_CHECKSUMMED_HEADER_LEN, d + OTRK_HEADER_LEN, out.payload_length)
         == out.checksum;
}

/**
 * @brief Writes an OTrk header for the given payload into `out` (16 bytes),
 * checksum included. The payload itself is not copied.
 */
inline void otrk_write_header(
    uint8_t* out, uint8_t encoding, uint16_t sequence, uint32_t segment_offset,
    bool last_segment, const uint8_t* payload, uint16_t payload_length) noexcept
{
  std::memcpy(out, "OTrk", 4);
  out[4] = 0;
  out[5] = encoding;
  otrk_write_u16_be(out + 6, sequence);
  otrk_write_u32_be(out + 8, segment_offset);
  otrk_write_u16_be(
      out + 12, uint16_t((last_segment ? 0x8000 : 0) | (payload_length & 0x7FFF)));
  otrk_write_u16_be(
      out + 14, fletcher16(out, OTRK_CHECKSUMMED_HEADER_LEN, payload, payload_length));
}
}
