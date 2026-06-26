#pragma once

#include "OpenXRDevice.hpp"
#include "OpenXRSpecificSettings.hpp"

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <QString>
#include <QVariant>

namespace OpenXR
{

class OpenXRDeviceInterface final : public Device::OwningDeviceInterface
{
public:
  OpenXRDeviceInterface(
      const Device::DeviceSettings& settings,
      const ossia::net::network_context_ptr& ctx)
      : Device::OwningDeviceInterface{settings}
      , m_ctx{ctx}
  {
    m_capas.canRefreshTree = true;
    m_capas.canSerialize = false;
    m_capas.hasCallbacks = false;
    m_capas.canRenameNode = false;
    m_capas.canSetProperties = false;
  }

  ~OpenXRDeviceInterface() override { }

  bool reconnect() override
  {
    disconnect();

    try
    {
      const auto& set = settings().deviceSpecificSettings.value<OpenXRSpecificSettings>();

      auto protocol = std::make_unique<OpenXRProtocol>(m_ctx, set);
      auto dev = std::make_unique<OpenXRDevice>(std::move(protocol), settings().name.toStdString());

      m_dev = std::move(dev);
      deviceChanged(nullptr, m_dev.get());
    }
    catch (const std::exception& e)
    {
      qDebug() << "OpenXR error: " << e.what();
      return false;
    }
    catch (...)
    {
      return false;
    }

    return connected();
  }

private:
  ossia::net::network_context_ptr m_ctx;
};

class OpenXRProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("a7b3c4d5-e6f7-8901-2345-6789abcdef01")

public:
  QString prettyName() const noexcept override { return QObject::tr("OpenXR"); }
  QString category() const noexcept override { return StandardCategories::tracking; }

  Device::DeviceEnumerators
  getEnumerators(const score::DocumentContext& ctx) const override
  {
    return {};
  }

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override
  {
    return new OpenXRDeviceInterface(settings, plugin.networkContext());
  }

  const Device::DeviceSettings& defaultSettings() const noexcept override
  {
    static const Device::DeviceSettings settings = [this] {
      Device::DeviceSettings s;
      s.protocol = concreteKey();
      s.name = "OpenXR";
      OpenXRSpecificSettings specif;
      s.deviceSpecificSettings = QVariant::fromValue(specif);
      return s;
    }();
    return settings;
  }

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx,
      QWidget* parent) override
  {
    return nullptr;
  }

  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings& set,
      const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx,
      QWidget* parent) override
  {
    return nullptr;
  }

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override
  {
    return makeProtocolSpecificSettings_T<OpenXRSpecificSettings>(visitor);
  }

  void serializeProtocolSpecificSettings(
      const QVariant& data, const VisitorVariant& visitor) const override
  {
    serializeProtocolSpecificSettings_T<OpenXRSpecificSettings>(data, visitor);
  }

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override
  {
    return false; // Only one OpenXR instance allowed
  }
};

}
