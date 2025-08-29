#include "TUIOProtocolFactory.hpp"
#include "TUIODevice.hpp"
#include "TUIOProtocolSettingsWidget.hpp"
#include "TUIOSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia/network/context.hpp>

namespace TUIO
{

QString TUIOProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("TUIO");
}

QString TUIOProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

Device::DeviceInterface* TUIOProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new TUIODevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& TUIOProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "TUIO";
    TUIOSpecificSettings spec;
    s.deviceSpecificSettings = QVariant::fromValue(spec);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* TUIOProtocolFactory::makeSettingsWidget()
{
  return new TUIOProtocolSettingsWidget;
}

QVariant TUIOProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<TUIOSpecificSettings>(visitor);
}

void TUIOProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<TUIOSpecificSettings>(data, visitor);
}

bool TUIOProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  auto a_set = a.deviceSpecificSettings.value<TUIOSpecificSettings>();
  auto b_set = b.deviceSpecificSettings.value<TUIOSpecificSettings>();
  return a_set.port == b_set.port;
}

}