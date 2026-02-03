#include "PSNProtocolFactory.hpp"
#include "PSNDevice.hpp"
#include "PSNProtocolSettingsWidget.hpp"
#include "PSNSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia/network/context.hpp>

namespace PSN
{

QString PSNProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("PSN");
}

QString PSNProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

Device::DeviceInterface* PSNProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new PSNDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& PSNProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "PSN";
    PSNSpecificSettings spec;
    s.deviceSpecificSettings = QVariant::fromValue(spec);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* PSNProtocolFactory::makeSettingsWidget()
{
  return new PSNProtocolSettingsWidget;
}

QVariant PSNProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<PSNSpecificSettings>(visitor);
}

void PSNProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<PSNSpecificSettings>(data, visitor);
}

bool PSNProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  auto a_set = a.deviceSpecificSettings.value<PSNSpecificSettings>();
  auto b_set = b.deviceSpecificSettings.value<PSNSpecificSettings>();
  return a_set.port == b_set.port && a_set.multicastAddress == b_set.multicastAddress;
}

}
