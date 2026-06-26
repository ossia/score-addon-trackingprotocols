#pragma once

#include <Explorer/DefaultProtocolFactory.hpp>

namespace OpenTrackIO
{
class OpenTrackIOProtocolFactory final : public Protocols::DefaultProtocolFactory
{
  SCORE_CONCRETE("b0c4d6a8-1f3e-4c5a-9e7b-0a2c4d6e8f01")

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
