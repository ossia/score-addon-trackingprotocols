#pragma once

#include <Device/Protocol/DeviceInterface.hpp>

namespace ossia::net
{
struct network_context;
using network_context_ptr = std::shared_ptr<network_context>;
}

namespace RTTrP
{
class RTTrPProtocol;
struct RTTrPSpecificSettings;

class RTTrPDevice final : public Device::OwningDeviceInterface
{
public:
  RTTrPDevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);

  ~RTTrPDevice();

  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
  RTTrPProtocol* m_protocol{nullptr};
};

}
