#pragma once

#include "OpenTrackIOSpecificSettings.hpp"

#include <ossia/network/base/protocol.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/sockets/udp_socket.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace opentrackio
{
struct OpenTrackIOSample;
}

namespace OpenTrackIO
{

class OpenTrackIOProtocol final : public ossia::net::protocol_base
{
public:
  OpenTrackIOProtocol(
      const ossia::net::network_context_ptr& ctx,
      const OpenTrackIOSpecificSettings& settings);

  ~OpenTrackIOProtocol();

  void set_device(ossia::net::device_base& dev) override;

  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value&) override { return false; }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base&) override { return false; }

private:
  // One listener per source number. Each has its own UDP socket joined to
  // 239.135.1.<sourceNumber> and its own reassembly state so segments from
  // different sources don't interleave.
  struct SourceListener
  {
    int source_number{0};
    std::unique_ptr<ossia::net::udp_receive_socket> socket;
    std::vector<uint8_t> reassembly;
    uint16_t prev_sequence{0};
    bool have_prev_sequence{false};
    uint8_t current_encoding{0};
  };

  void create_device_tree(ossia::net::node_base& root);
  void create_source_slot(ossia::net::node_base& sourcesNode, int sourceNumber);

  void start_listeners();
  void stop_listeners();
  void on_datagram(SourceListener& s, const char* data, std::size_t size);
  void apply_sample(
      const opentrackio::OpenTrackIOSample& sample, int sourceNumber);

  ossia::net::node_base* get_or_create_source_slot(int sourceNumber);

  ossia::net::network_context_ptr m_ctx;
  ossia::net::device_base* m_device{nullptr};
  OpenTrackIOSpecificSettings m_settings;
  std::vector<std::unique_ptr<SourceListener>> m_listeners;
};

}
