#pragma once

#include <Explorer/DefaultProtocolFactory.hpp>

namespace RTTrP
{
class RTTrPProtocolFactory final : public Protocols::DefaultProtocolFactory
{
  SCORE_CONCRETE("b4e5d2f1-3a8c-4b7e-9c0d-1f2e3a4b5c6d")

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
