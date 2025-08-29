#pragma once

#include <Explorer/DefaultProtocolFactory.hpp>

namespace TUIO
{
class TUIOProtocolFactory final : public Protocols::DefaultProtocolFactory
{
  SCORE_CONCRETE("5e7b9c82-86e4-4e52-b01d-fa4e5cbd413a")
  
public:
  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  
  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;
  
  const Device::DeviceSettings& defaultSettings() const noexcept override;
  
  Device::ProtocolSettingsWidget* makeSettingsWidget() override;
  
  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;
  
  void serializeProtocolSpecificSettings(
      const QVariant& data,
      const VisitorVariant& visitor) const override;
  
  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override;
};
}