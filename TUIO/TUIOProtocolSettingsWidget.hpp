#pragma once

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

class QLineEdit;
class QSpinBox;
class QComboBox;

namespace TUIO
{
class TUIOProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  TUIOProtocolSettingsWidget(QWidget* parent = nullptr);
  
  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  QLineEdit* m_deviceNameEdit{};
  QSpinBox* m_portEdit{};
  QSpinBox* m_numObjectsEdit{};
  QSpinBox* m_numCursorsEdit{};
  QSpinBox* m_numBlobsEdit{};
  QComboBox* m_versionCombo{};
};
}