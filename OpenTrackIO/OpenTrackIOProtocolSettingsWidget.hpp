#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

class QLineEdit;
class QSpinBox;
class QCheckBox;

namespace OpenTrackIO
{
class OpenTrackIOProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  OpenTrackIOProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  QLineEdit* m_deviceNameEdit{};
  QLineEdit* m_multicastBaseEdit{};
  QSpinBox*  m_portEdit{};
  QSpinBox*  m_minSourceEdit{};
  QSpinBox*  m_maxSourceEdit{};
  QCheckBox* m_enableCameraCheck{};
  QCheckBox* m_enableLensCheck{};
  QCheckBox* m_enableTimingCheck{};
  QCheckBox* m_enableGlobalStageCheck{};
  QCheckBox* m_acceptJSONCheck{};
  QCheckBox* m_acceptCBORCheck{};
};

}
