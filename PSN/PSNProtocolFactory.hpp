#pragma once

#include <Explorer/DefaultProtocolFactory.hpp>

namespace PSN
{
class PSNProtocolFactory final : public Protocols::DefaultProtocolFactory
{
  SCORE_CONCRETE("a3f2c1e4-7d9b-4e5f-8a6c-2b1d0e9f8c7d")

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
