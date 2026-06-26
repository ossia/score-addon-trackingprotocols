#include "OpenXRProtocolSettingsWidget.hpp"
#include "OpenXRProtocolFactory.hpp"
#include "OpenXRSpecificSettings.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVariant>

namespace OpenXR
{

OpenXRProtocolSettingsWidget::OpenXRProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new QLineEdit(this);
  m_deviceNameEdit->setText("OpenXR");

  m_enableControllers = new QCheckBox(this);
  m_enableControllers->setChecked(true);

  m_enableHandTracking = new QCheckBox(this);
  m_enableHandTracking->setChecked(true);

  m_pollRate = new QSpinBox(this);
  m_pollRate->setRange(1, 100);
  m_pollRate->setValue(11);
  m_pollRate->setSuffix(" ms");

  auto layout = new QFormLayout;
  layout->addRow(tr("Device name"), m_deviceNameEdit);
  layout->addRow(tr("Enable Controllers"), m_enableControllers);
  layout->addRow(tr("Enable Hand Tracking"), m_enableHandTracking);
  layout->addRow(tr("Poll Rate"), m_pollRate);

  auto info = new QLabel(tr(
      "OpenXR Protocol\n\n"
      "Connects to an OpenXR runtime to receive VR tracking data.\n\n"
      "Controllers:\n"
      "- Left/Right hand position and orientation\n"
      "- Trigger and grab values\n\n"
      "Hand Tracking (if supported):\n"
      "- Full 26-joint skeleton per hand\n"
      "- Position, orientation, and radius for each joint\n\n"
      "Requires an active OpenXR runtime (SteamVR, Monado, etc.)"));
  info->setWordWrap(true);
  layout->addRow(info);

  setLayout(layout);
}

Device::DeviceSettings OpenXRProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = OpenXRProtocolFactory::static_concreteKey();

  OpenXRSpecificSettings spec;
  spec.enableControllers = m_enableControllers->isChecked();
  spec.enableHandTracking = m_enableHandTracking->isChecked();
  spec.pollRateMs = m_pollRate->value();

  s.deviceSpecificSettings = QVariant::fromValue(spec);
  return s;
}

void OpenXRProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);

  if (settings.deviceSpecificSettings.canConvert<OpenXRSpecificSettings>())
  {
    auto spec = settings.deviceSpecificSettings.value<OpenXRSpecificSettings>();
    m_enableControllers->setChecked(spec.enableControllers);
    m_enableHandTracking->setChecked(spec.enableHandTracking);
    m_pollRate->setValue(spec.pollRateMs);
  }
}

}
