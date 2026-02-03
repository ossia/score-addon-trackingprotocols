#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

class QLineEdit;
class QSpinBox;
class QCheckBox;

namespace PSN
{
class PSNProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  PSNProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  QLineEdit* m_deviceNameEdit{};
  QLineEdit* m_multicastAddressEdit{};
  QSpinBox* m_portEdit{};
  QSpinBox* m_numTrackersEdit{};
  QCheckBox* m_enableVelocityCheck{};
  QCheckBox* m_enableAccelerationCheck{};
  QCheckBox* m_enableOrientationCheck{};
  QCheckBox* m_enableTargetPositionCheck{};
};

}
