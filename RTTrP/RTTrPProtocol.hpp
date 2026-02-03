#pragma once

#include "RTTrPSpecificSettings.hpp"
#include "../Common/TrackingTypes.hpp"
#include "../Common/TrackingSlotManager.hpp"

#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/sockets/udp_socket.hpp>

#include <chrono>

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
  ZoneMod = 0x23
};

// Parsed trackable data
struct ParsedTrackable
{
  std::string name;
  uint32_t timestamp{0};

  // Centroid position
  bool has_centroid{false};
  double x{}, y{}, z{};
  uint16_t latency{0};

  // Quaternion orientation
  bool has_quaternion{false};
  double qx{}, qy{}, qz{}, qw{};

  // Euler orientation
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

  // Zones
  std::vector<std::string> zones;
};

class RTTrPProtocol final : public ossia::net::protocol_base
{
public:
  explicit RTTrPProtocol(
      const ossia::net::network_context_ptr& ctx,
      const RTTrPSpecificSettings& settings);

  ~RTTrPProtocol();

  void set_device(ossia::net::device_base& dev) override;

  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value&) override { return false; }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base& node_base) override { return false; }

private:
  void setup_receive_socket();
  void stop_receive();
  void on_received_data(const char* data, std::size_t size);

  bool parse_packet(const uint8_t* data, std::size_t size);
  bool parse_trackable(const uint8_t* data, std::size_t size, ParsedTrackable& out);

  void create_device_tree(ossia::net::node_base& root);
  void update_trackable_parameters(int slot, const ParsedTrackable& trackable);
  void update_zone_parameter(const std::string& zone_name, bool occupied);

  // Binary parsing helpers
  template<typename T>
  static T read_value(const uint8_t*& ptr, bool swap_bytes);

  ossia::net::network_context_ptr m_ctx;
  std::unique_ptr<ossia::net::udp_receive_socket> m_receive_socket;
  ossia::net::device_base* m_device{nullptr};

  RTTrPSpecificSettings m_settings;

  // Slot management for trackables (name-based)
  Tracking::SlotManager<Tracking::TrackedObject> m_trackable_slots;

  // Zone tracking
  ossia::hash_map<std::string, ossia::net::node_base*> m_zone_nodes;

  // Frame info
  uint32_t m_packet_id{0};
  bool m_little_endian{true};
};

}
