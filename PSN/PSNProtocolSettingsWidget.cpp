#include "PSNProtocolSettingsWidget.hpp"
#include "PSNProtocolFactory.hpp"
#include "PSNSpecificSettings.hpp"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QVariant>

namespace PSN
{

PSNProtocolSettingsWidget::PSNProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new QLineEdit(this);
  m_deviceNameEdit->setText("PSN");

  m_multicastAddressEdit = new QLineEdit(this);
  m_multicastAddressEdit->setText("236.10.10.10");

  m_portEdit = new QSpinBox(this);
  m_portEdit->setRange(1, 65535);
  m_portEdit->setValue(56565);

  m_numTrackersEdit = new QSpinBox(this);
  m_numTrackersEdit->setRange(1, 128);
  m_numTrackersEdit->setValue(16);

  m_enableVelocityCheck = new QCheckBox(this);
  m_enableVelocityCheck->setChecked(true);

  m_enableAccelerationCheck = new QCheckBox(this);
  m_enableAccelerationCheck->setChecked(true);

  m_enableOrientationCheck = new QCheckBox(this);
  m_enableOrientationCheck->setChecked(true);

  m_enableTargetPositionCheck = new QCheckBox(this);
  m_enableTargetPositionCheck->setChecked(false);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device name"), m_deviceNameEdit);
  layout->addRow(tr("Multicast Address"), m_multicastAddressEdit);
  layout->addRow(tr("UDP Port"), m_portEdit);
  layout->addRow(tr("Number of Trackers"), m_numTrackersEdit);
  layout->addRow(tr("Enable Velocity"), m_enableVelocityCheck);
  layout->addRow(tr("Enable Acceleration"), m_enableAccelerationCheck);
  layout->addRow(tr("Enable Orientation"), m_enableOrientationCheck);
  layout->addRow(tr("Enable Target Position"), m_enableTargetPositionCheck);

  auto info = new QLabel(tr(
      "PosiStageNet (PSN) Protocol\n\n"
      "Receives PSN tracker data on the specified UDP multicast group.\n"
      "Default: 236.10.10.10:56565\n\n"
      "PSN is commonly used for:\n"
      "- Blacktrax tracking systems\n"
      "- Stage tracking automation\n"
      "- Live entertainment tracking\n\n"
      "Data includes:\n"
      "- Position (X, Y, Z in mm)\n"
      "- Orientation (Roll, Pitch, Yaw)\n"
      "- Velocity and Acceleration\n"
      "- Target position (automation)"));
  info->setWordWrap(true);
  layout->addRow(info);

  setLayout(layout);
}

Device::DeviceSettings PSNProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = PSNProtocolFactory::static_concreteKey();

  PSNSpecificSettings spec;
  spec.multicastAddress = m_multicastAddressEdit->text();
  spec.port = m_portEdit->value();
  spec.numTrackers = m_numTrackersEdit->value();
  spec.enableVelocity = m_enableVelocityCheck->isChecked();
  spec.enableAcceleration = m_enableAccelerationCheck->isChecked();
  spec.enableOrientation = m_enableOrientationCheck->isChecked();
  spec.enableTargetPosition = m_enableTargetPositionCheck->isChecked();

  s.deviceSpecificSettings = QVariant::fromValue(spec);
  return s;
}

void PSNProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);

  if (settings.deviceSpecificSettings.canConvert<PSNSpecificSettings>())
  {
    auto spec = settings.deviceSpecificSettings.value<PSNSpecificSettings>();
    m_multicastAddressEdit->setText(spec.multicastAddress);
    m_portEdit->setValue(spec.port);
    m_numTrackersEdit->setValue(spec.numTrackers);
    m_enableVelocityCheck->setChecked(spec.enableVelocity);
    m_enableAccelerationCheck->setChecked(spec.enableAcceleration);
    m_enableOrientationCheck->setChecked(spec.enableOrientation);
    m_enableTargetPositionCheck->setChecked(spec.enableTargetPosition);
  }
}

}
