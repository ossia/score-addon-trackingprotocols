#include "PSNDevice.hpp"
#include "PSNProtocol.hpp"
#include "PSNSpecificSettings.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/context.hpp>

namespace PSN
{

PSNDevice::PSNDevice(
    const Device::DeviceSettings& settings,
    const ossia::net::network_context_ptr& ctx)
    : Device::OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.canRenameNode = true;
  m_capas.canSetProperties = false;
  m_capas.canRemoveNode = false;
  m_capas.canAddNode = false;
  m_capas.canSerialize = false;

  reconnect();
}

PSNDevice::~PSNDevice()
{
  disconnect();
}

bool PSNDevice::reconnect()
{
  disconnect();

  try
  {
    const auto& stgs = settings();
    const auto& psn_settings = stgs.deviceSpecificSettings.value<PSNSpecificSettings>();

    auto protocol = std::make_unique<PSNProtocol>(m_ctx, psn_settings);
    auto dev = std::make_unique<ossia::net::generic_device>(
        std::move(protocol), stgs.name.toStdString());

    m_protocol = static_cast<PSNProtocol*>(&dev->get_protocol());
    m_dev = std::move(dev);

    deviceChanged(nullptr, m_dev.get());
    return true;
  }
  catch (const std::exception& e)
  {
    qDebug() << "PSN connection error:" << e.what();
    return false;
  }
}

void PSNDevice::disconnect()
{
  if (m_dev)
  {
    m_protocol = nullptr;
    m_dev.reset();
  }
}

}
