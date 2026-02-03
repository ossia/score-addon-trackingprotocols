#include "RTTrPDevice.hpp"
#include "RTTrPProtocol.hpp"
#include "RTTrPSpecificSettings.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/context.hpp>

namespace RTTrP
{

RTTrPDevice::RTTrPDevice(
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

RTTrPDevice::~RTTrPDevice()
{
  disconnect();
}

bool RTTrPDevice::reconnect()
{
  disconnect();

  try
  {
    const auto& stgs = settings();
    const auto& rttrp_settings = stgs.deviceSpecificSettings.value<RTTrPSpecificSettings>();

    auto protocol = std::make_unique<RTTrPProtocol>(m_ctx, rttrp_settings);
    auto dev = std::make_unique<ossia::net::generic_device>(
        std::move(protocol), stgs.name.toStdString());

    m_protocol = static_cast<RTTrPProtocol*>(&dev->get_protocol());
    m_dev = std::move(dev);

    deviceChanged(nullptr, m_dev.get());
    return true;
  }
  catch (const std::exception& e)
  {
    qDebug() << "RTTrP connection error:" << e.what();
    return false;
  }
}

void RTTrPDevice::disconnect()
{
  if (m_dev)
  {
    m_protocol = nullptr;
    m_dev.reset();
  }
}

}
