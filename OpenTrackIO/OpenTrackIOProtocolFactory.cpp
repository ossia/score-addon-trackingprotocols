#include "OpenTrackIOProtocolFactory.hpp"
#include "OpenTrackIODevice.hpp"
#include "OpenTrackIOProtocolSettingsWidget.hpp"
#include "OpenTrackIOSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia/network/context.hpp>

namespace OpenTrackIO
{

QString OpenTrackIOProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("OpenTrackIO");
}

QString OpenTrackIOProtocolFactory::category() const noexcept
{
  return StandardCategories::tracking;
}

Device::DeviceInterface* OpenTrackIOProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new OpenTrackIODevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings& OpenTrackIOProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "OpenTrackIO";
    OpenTrackIOSpecificSettings spec;
    s.deviceSpecificSettings = QVariant::fromValue(spec);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* OpenTrackIOProtocolFactory::makeSettingsWidget()
{
  return new OpenTrackIOProtocolSettingsWidget;
}

QVariant OpenTrackIOProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<OpenTrackIOSpecificSettings>(visitor);
}

void OpenTrackIOProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<OpenTrackIOSpecificSettings>(data, visitor);
}

bool OpenTrackIOProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  auto a_set = a.deviceSpecificSettings.value<OpenTrackIOSpecificSettings>();
  auto b_set = b.deviceSpecificSettings.value<OpenTrackIOSpecificSettings>();
  // Compatible (can coexist) when they use different ports, or their source
  // number ranges don't overlap (i.e. they join disjoint multicast groups).
  if(a_set.port != b_set.port)
    return true;
  return a_set.maxSourceNumber < b_set.minSourceNumber
         || b_set.maxSourceNumber < a_set.minSourceNumber;
}

}
