#include "RTTrPProtocolFactory.hpp"
#include "RTTrPDevice.hpp"
#include "RTTrPProtocolSettingsWidget.hpp"
#include "RTTrPSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia/network/context.hpp>

namespace RTTrP
{

QString RTTrPProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("RTTrP");
}

QString RTTrPProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

Device::DeviceInterface* RTTrPProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new RTTrPDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& RTTrPProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "RTTrP";
    RTTrPSpecificSettings spec;
    s.deviceSpecificSettings = QVariant::fromValue(spec);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* RTTrPProtocolFactory::makeSettingsWidget()
{
  return new RTTrPProtocolSettingsWidget;
}

QVariant RTTrPProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<RTTrPSpecificSettings>(visitor);
}

void RTTrPProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<RTTrPSpecificSettings>(data, visitor);
}

bool RTTrPProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  auto a_set = a.deviceSpecificSettings.value<RTTrPSpecificSettings>();
  auto b_set = b.deviceSpecificSettings.value<RTTrPSpecificSettings>();
  return a_set.port == b_set.port;
}

}
