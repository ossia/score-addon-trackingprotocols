#pragma once

#include <Device/Protocol/DeviceInterface.hpp>

namespace ossia::net
{
struct network_context;
using network_context_ptr = std::shared_ptr<network_context>;
}

namespace PSN
{
class PSNProtocol;
struct PSNSpecificSettings;

class PSNDevice final : public Device::OwningDeviceInterface
{
public:
  PSNDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);

  ~PSNDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
  PSNProtocol* m_protocol{nullptr};
};

}
