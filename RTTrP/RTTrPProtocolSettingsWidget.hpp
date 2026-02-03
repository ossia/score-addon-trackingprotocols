#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

class QLineEdit;
class QSpinBox;
class QCheckBox;

namespace RTTrP
{
class RTTrPProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  RTTrPProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  QLineEdit* m_deviceNameEdit{};
  QSpinBox* m_portEdit{};
  QSpinBox* m_numTrackablesEdit{};
  QSpinBox* m_maxLEDsEdit{};
  QSpinBox* m_maxZonesEdit{};
  QCheckBox* m_enableQuaternionCheck{};
  QCheckBox* m_enableEulerCheck{};
  QCheckBox* m_enableVelocityCheck{};
  QCheckBox* m_enableAccelerationCheck{};
  QCheckBox* m_enableZonesCheck{};
};

}
