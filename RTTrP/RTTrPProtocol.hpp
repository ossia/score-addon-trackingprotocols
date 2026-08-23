#pragma once
#include "RTTrPParser.hpp"
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

  /**
   * @brief Decodes one RTTrPM datagram and applies it to the device tree.
   *
   * This is what the receive socket feeds; it is public so that the protocol
   * can be driven from captured or synthesized packets.
   * @return false if the packet was rejected (bad header, duplicate packet id).
   */
  bool process_packet(const char* data, std::size_t size);

private:
  void setup_receive_socket();
  void stop_receive();
  void on_received_data(const char* data, std::size_t size);

  void create_device_tree(ossia::net::node_base& root);
  void update_trackable_parameters(int slot, const ParsedTrackable& trackable);
  void update_zone_parameter(const std::string& zone_name, bool occupied);

  ossia::net::network_context_ptr m_ctx;
  std::unique_ptr<ossia::net::udp_receive_socket> m_receive_socket;
  ossia::net::device_base* m_device{nullptr};
  RTTrPSpecificSettings m_settings;

  // Slot management for trackables (name-based)
  Tracking::SlotManager<Tracking::TrackedObject> m_trackable_slots;

  // Zone tracking
  ossia::hash_map<std::string, ossia::net::node_base*> m_zone_nodes;

  // Reused across packets to avoid reallocating the trackable list
  ParsedPacket m_parsed;

  // Duplicate-packet rejection: RTTrP's 32-bit packet_id increments once per
  // sent packet; repeats mean retransmits or echo loops.
  uint32_t m_prev_packet_id{0};
  bool m_have_prev_packet_id{false};
};
}
