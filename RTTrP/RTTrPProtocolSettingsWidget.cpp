#include "RTTrPProtocolSettingsWidget.hpp"
#include "RTTrPProtocolFactory.hpp"
#include "RTTrPSpecificSettings.hpp"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QVariant>

namespace RTTrP
{

RTTrPProtocolSettingsWidget::RTTrPProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new QLineEdit(this);
  m_deviceNameEdit->setText("RTTrP");

  m_portEdit = new QSpinBox(this);
  m_portEdit->setRange(1, 65535);
  m_portEdit->setValue(24002);

  m_numTrackablesEdit = new QSpinBox(this);
  m_numTrackablesEdit->setRange(1, 128);
  m_numTrackablesEdit->setValue(16);

  m_maxLEDsEdit = new QSpinBox(this);
  m_maxLEDsEdit->setRange(0, 32);
  m_maxLEDsEdit->setValue(8);

  m_maxZonesEdit = new QSpinBox(this);
  m_maxZonesEdit->setRange(0, 64);
  m_maxZonesEdit->setValue(8);

  m_enableQuaternionCheck = new QCheckBox(this);
  m_enableQuaternionCheck->setChecked(true);

  m_enableEulerCheck = new QCheckBox(this);
  m_enableEulerCheck->setChecked(false);

  m_enableVelocityCheck = new QCheckBox(this);
  m_enableVelocityCheck->setChecked(true);

  m_enableAccelerationCheck = new QCheckBox(this);
  m_enableAccelerationCheck->setChecked(true);

  m_enableZonesCheck = new QCheckBox(this);
  m_enableZonesCheck->setChecked(true);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device name"), m_deviceNameEdit);
  layout->addRow(tr("UDP Port"), m_portEdit);
  layout->addRow(tr("Number of Trackables"), m_numTrackablesEdit);
  layout->addRow(tr("Max LEDs per Trackable"), m_maxLEDsEdit);
  layout->addRow(tr("Max Zones"), m_maxZonesEdit);
  layout->addRow(tr("Enable Quaternion"), m_enableQuaternionCheck);
  layout->addRow(tr("Enable Euler Angles"), m_enableEulerCheck);
  layout->addRow(tr("Enable Velocity"), m_enableVelocityCheck);
  layout->addRow(tr("Enable Acceleration"), m_enableAccelerationCheck);
  layout->addRow(tr("Enable Zones"), m_enableZonesCheck);

  auto info = new QLabel(tr(
      "RTTrP (Real-Time Tracking Protocol)\n\n"
      "Receives RTTrP tracking data on the specified UDP port.\n"
      "Default port: 24002\n\n"
      "RTTrP is commonly used for:\n"
      "- BlackTrax tracking systems\n"
      "- Motion capture integration\n"
      "- Real-time performer tracking\n\n"
      "Data includes:\n"
      "- Position (X, Y, Z in mm)\n"
      "- Orientation (Quaternion or Euler)\n"
      "- LED marker positions\n"
      "- Zone collision detection"));
  info->setWordWrap(true);
  layout->addRow(info);

  setLayout(layout);
}

Device::DeviceSettings RTTrPProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = RTTrPProtocolFactory::static_concreteKey();

  RTTrPSpecificSettings spec;
  spec.port = m_portEdit->value();
  spec.numTrackables = m_numTrackablesEdit->value();
  spec.maxLEDsPerTrackable = m_maxLEDsEdit->value();
  spec.maxZones = m_maxZonesEdit->value();
  spec.enableQuaternion = m_enableQuaternionCheck->isChecked();
  spec.enableEuler = m_enableEulerCheck->isChecked();
  spec.enableVelocity = m_enableVelocityCheck->isChecked();
  spec.enableAcceleration = m_enableAccelerationCheck->isChecked();
  spec.enableZones = m_enableZonesCheck->isChecked();

  s.deviceSpecificSettings = QVariant::fromValue(spec);
  return s;
}

void RTTrPProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);

  if (settings.deviceSpecificSettings.canConvert<RTTrPSpecificSettings>())
  {
    auto spec = settings.deviceSpecificSettings.value<RTTrPSpecificSettings>();
    m_portEdit->setValue(spec.port);
    m_numTrackablesEdit->setValue(spec.numTrackables);
    m_maxLEDsEdit->setValue(spec.maxLEDsPerTrackable);
    m_maxZonesEdit->setValue(spec.maxZones);
    m_enableQuaternionCheck->setChecked(spec.enableQuaternion);
    m_enableEulerCheck->setChecked(spec.enableEuler);
    m_enableVelocityCheck->setChecked(spec.enableVelocity);
    m_enableAccelerationCheck->setChecked(spec.enableAcceleration);
    m_enableZonesCheck->setChecked(spec.enableZones);
  }
}

}
