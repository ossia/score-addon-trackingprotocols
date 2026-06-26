#pragma once

#include <Device/Protocol/DeviceInterface.hpp>

namespace ossia::net
{
struct network_context;
using network_context_ptr = std::shared_ptr<network_context>;
}

namespace OpenTrackIO
{
class OpenTrackIOProtocol;
struct OpenTrackIOSpecificSettings;

class OpenTrackIODevice final : public Device::OwningDeviceInterface
{
public:
  OpenTrackIODevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);

  ~OpenTrackIODevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
  OpenTrackIOProtocol* m_protocol{nullptr};
};

}
