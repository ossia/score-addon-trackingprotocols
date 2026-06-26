#pragma once

#include "PSNSpecificSettings.hpp"
#include "../Common/TrackingTypes.hpp"
#include "../Common/TrackingSlotManager.hpp"

#include <ossia/detail/logger.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/sockets/udp_socket.hpp>

#include <psn_decoder.hpp>

#include <chrono>

namespace PSN
{

class PSNProtocol final : public ossia::net::protocol_base
{
public:
  explicit PSNProtocol(
      const ossia::net::network_context_ptr& ctx,
      const PSNSpecificSettings& settings);

  ~PSNProtocol();

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

  void process_info_packet(const ::psn::psn_decoder::info_t& info);
  void process_data_packet(const ::psn::psn_decoder::data_t& data);

  void create_device_tree(ossia::net::node_base& root);
  void update_tracker_parameters(int slot, const ::psn::tracker& tracker);

  ossia::net::network_context_ptr m_ctx;
  std::unique_ptr<ossia::net::udp_receive_socket> m_receive_socket;
  ossia::net::device_base* m_device{nullptr};

  PSNSpecificSettings m_settings;
  ::psn::psn_decoder m_decoder;

  // Slot management for trackers
  Tracking::SlotManager<Tracking::TrackedObject> m_tracker_slots;

  // Frame info
  uint8_t m_frame_id{0};
  uint64_t m_timestamp{0};
  std::string m_system_name;

  // Duplicate-packet rejection: PSN's header frame_id increments once per
  // transmitted frame; repeats mean retransmits or loopback echoes.
  uint8_t m_prev_frame_id{0};
  bool m_have_prev_frame_id{false};
};

}
