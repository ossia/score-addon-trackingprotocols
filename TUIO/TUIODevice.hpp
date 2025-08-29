#pragma once

#include <Device/Protocol/DeviceInterface.hpp>

namespace ossia::net
{
struct network_context;
using network_context_ptr = std::shared_ptr<network_context>;
}

namespace TUIO
{
class TUIOProtocol;
struct TUIOSpecificSettings;

class TUIODevice final : public Device::OwningDeviceInterface
{
public:
  TUIODevice(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx);
  
  ~TUIODevice();
  
  bool reconnect() override;
  void disconnect() override;

private:
  const ossia::net::network_context_ptr& m_ctx;
  TUIOProtocol* m_protocol{nullptr};
};
}