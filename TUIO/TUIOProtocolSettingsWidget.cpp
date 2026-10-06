#include "TUIOProtocolSettingsWidget.hpp"
#include "TUIOProtocolFactory.hpp"
#include "TUIOSpecificSettings.hpp"

#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
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
  
  m_numObjectsEdit = new QSpinBox(this);
  m_numObjectsEdit->setRange(1, 64);
  m_numObjectsEdit->setValue(8);
  
  m_numCursorsEdit = new QSpinBox(this);
  m_numCursorsEdit->setRange(1, 64);
  m_numCursorsEdit->setValue(8);
  
  m_numBlobsEdit = new QSpinBox(this);
  m_numBlobsEdit->setRange(1, 64);
  m_numBlobsEdit->setValue(8);
  
  m_versionCombo = new QComboBox(this);
  m_versionCombo->addItem("TUIO 1.1");
  m_versionCombo->addItem("TUIO 2.0");
  m_versionCombo->setCurrentIndex(0);
  
  auto layout = new QFormLayout;
  layout->addRow(tr("Device name"), m_deviceNameEdit);
  layout->addRow(tr("UDP Port"), m_portEdit);
  layout->addRow(tr("Number of Objects"), m_numObjectsEdit);
  layout->addRow(tr("Number of Cursors"), m_numCursorsEdit);
  layout->addRow(tr("Number of Blobs"), m_numBlobsEdit);
  layout->addRow(tr("Protocol Version"), m_versionCombo);

  setLayout(layout);
}

Device::DeviceSettings TUIOProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = TUIOProtocolFactory::static_concreteKey();
  
  TUIOSpecificSettings spec;
  spec.port = m_portEdit->value();
  spec.numObjects = m_numObjectsEdit->value();
  spec.numCursors = m_numCursorsEdit->value();
  spec.numBlobs = m_numBlobsEdit->value();
  spec.version = m_versionCombo->currentIndex() == 0 ? TUIOVersion::V1_1 : TUIOVersion::V2_0;
  
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
    m_numObjectsEdit->setValue(spec.numObjects);
    m_numCursorsEdit->setValue(spec.numCursors);
    m_numBlobsEdit->setValue(spec.numBlobs);
    m_versionCombo->setCurrentIndex(spec.version == TUIOVersion::V1_1 ? 0 : 1);
  }
}

}