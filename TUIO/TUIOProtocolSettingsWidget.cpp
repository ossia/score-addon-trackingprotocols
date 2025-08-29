#include "TUIOProtocolSettingsWidget.hpp"
#include "TUIOProtocolFactory.hpp"
#include "TUIOSpecificSettings.hpp"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVariant>

namespace TUIO
{

TUIOProtocolSettingsWidget::TUIOProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new QLineEdit(this);
  m_deviceNameEdit->setText("TUIO");
  
  m_portEdit = new QSpinBox(this);
  m_portEdit->setRange(1, 65535);
  m_portEdit->setValue(3333);
  
  auto layout = new QFormLayout;
  layout->addRow(tr("Device name"), m_deviceNameEdit);
  layout->addRow(tr("UDP Port"), m_portEdit);
  
  auto info = new QLabel(tr(
      "TUIO 1.1 Protocol\n"
      "Receives TUIO messages on the specified UDP port.\n"
      "Default port: 3333\n\n"
      "Supported profiles:\n"
      "- 2Dobj: Tangible objects with position and rotation\n"
      "- 2Dcur: Touch cursors with position\n"
      "- 2Dblb: Blob regions with position, size and rotation"));
  info->setWordWrap(true);
  layout->addRow(info);
  
  setLayout(layout);
}

Device::DeviceSettings TUIOProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = TUIOProtocolFactory::static_concreteKey();
  
  TUIOSpecificSettings spec;
  spec.port = m_portEdit->value();
  
  s.deviceSpecificSettings = QVariant::fromValue(spec);
  return s;
}

void TUIOProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  
  if (settings.deviceSpecificSettings.canConvert<TUIOSpecificSettings>())
  {
    auto spec = settings.deviceSpecificSettings.value<TUIOSpecificSettings>();
    m_portEdit->setValue(spec.port);
  }
}

}