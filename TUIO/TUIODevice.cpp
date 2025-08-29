#include "TUIODevice.hpp"
#include "TUIOProtocol.hpp"
#include "TUIOSpecificSettings.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/context.hpp>

namespace TUIO
{

TUIODevice::TUIODevice(
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

TUIODevice::~TUIODevice()
{
  disconnect();
}

bool TUIODevice::reconnect()
{
  disconnect();
  
  try
  {
    const auto& stgs = settings();
    const auto& tuio_settings = stgs.deviceSpecificSettings.value<TUIOSpecificSettings>();
    
    auto protocol = std::make_unique<TUIOProtocol>(
        m_ctx, 
        tuio_settings.port,
        tuio_settings.numObjects,
        tuio_settings.numCursors,
        tuio_settings.numBlobs);
    auto dev = std::make_unique<ossia::net::generic_device>(
        std::move(protocol), stgs.name.toStdString());
    
    m_protocol = static_cast<TUIOProtocol*>(&dev->get_protocol());
    m_dev = std::move(dev);
    
    deviceChanged(nullptr, m_dev.get());
    return true;
  }
  catch (const std::exception& e)
  {
    qDebug() << "TUIO connection error:" << e.what();
    return false;
  }
}

void TUIODevice::disconnect()
{
  if (m_dev)
  {
    m_protocol = nullptr;
    m_dev.reset();
  }
}

}