#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

class QCheckBox;
class QSpinBox;
class QLineEdit;

namespace OpenXR
{

class OpenXRProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  OpenXRProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  QLineEdit* m_deviceNameEdit{};
  QCheckBox* m_enableControllers{};
  QCheckBox* m_enableHandTracking{};
  QSpinBox* m_pollRate{};
};

}
