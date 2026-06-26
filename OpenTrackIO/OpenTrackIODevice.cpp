#include "OpenTrackIODevice.hpp"
#include "OpenTrackIOProtocol.hpp"
#include "OpenTrackIOSpecificSettings.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>

namespace OpenTrackIO
{

OpenTrackIODevice::OpenTrackIODevice(
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

OpenTrackIODevice::~OpenTrackIODevice()
{
  disconnect();
}

bool OpenTrackIODevice::reconnect()
{
  disconnect();

  try
  {
    const auto& stgs = settings();
    const auto& spec = stgs.deviceSpecificSettings.value<OpenTrackIOSpecificSettings>();

    auto protocol = std::make_unique<OpenTrackIOProtocol>(m_ctx, spec);
    auto dev = std::make_unique<ossia::net::generic_device>(
        std::move(protocol), stgs.name.toStdString());

    m_protocol = static_cast<OpenTrackIOProtocol*>(&dev->get_protocol());
    m_dev = std::move(dev);

    deviceChanged(nullptr, m_dev.get());
    return true;
  }
  catch (const std::exception& e)
  {
    qDebug() << "OpenTrackIO connection error:" << e.what();
    return false;
  }
}

void OpenTrackIODevice::disconnect()
{
  if (m_dev)
  {
    m_protocol = nullptr;
    m_dev.reset();
  }
}

}
