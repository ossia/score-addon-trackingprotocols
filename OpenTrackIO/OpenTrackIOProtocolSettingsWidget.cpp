#include "OpenTrackIOProtocolSettingsWidget.hpp"
#include "OpenTrackIOProtocolFactory.hpp"
#include "OpenTrackIOSpecificSettings.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVariant>

namespace OpenTrackIO
{

OpenTrackIOProtocolSettingsWidget::OpenTrackIOProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new QLineEdit(this);
  m_deviceNameEdit->setText("OpenTrackIO");

  m_multicastBaseEdit = new QLineEdit(this);
  m_multicastBaseEdit->setText("239.135.1");

  m_portEdit = new QSpinBox(this);
  m_portEdit->setRange(1, 65535);
  m_portEdit->setValue(55555);

  m_minSourceEdit = new QSpinBox(this);
  m_minSourceEdit->setRange(1, 200);
  m_minSourceEdit->setValue(1);

  m_maxSourceEdit = new QSpinBox(this);
  m_maxSourceEdit->setRange(1, 200);
  m_maxSourceEdit->setValue(1);

  m_enableCameraCheck = new QCheckBox(this);
  m_enableCameraCheck->setChecked(true);

  m_enableLensCheck = new QCheckBox(this);
  m_enableLensCheck->setChecked(true);

  m_enableTimingCheck = new QCheckBox(this);
  m_enableTimingCheck->setChecked(true);

  m_enableGlobalStageCheck = new QCheckBox(this);
  m_enableGlobalStageCheck->setChecked(false);

  m_acceptJSONCheck = new QCheckBox(this);
  m_acceptJSONCheck->setChecked(true);

  m_acceptCBORCheck = new QCheckBox(this);
  m_acceptCBORCheck->setChecked(false);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device name"), m_deviceNameEdit);
  layout->addRow(tr("Multicast base (239.135.1)"), m_multicastBaseEdit);
  layout->addRow(tr("UDP port"), m_portEdit);
  layout->addRow(tr("Min source number"), m_minSourceEdit);
  layout->addRow(tr("Max source number"), m_maxSourceEdit);
  layout->addRow(tr("Enable camera metadata"), m_enableCameraCheck);
  layout->addRow(tr("Enable lens metadata"), m_enableLensCheck);
  layout->addRow(tr("Enable timing / timecode"), m_enableTimingCheck);
  layout->addRow(tr("Enable globalStage (geodetic)"), m_enableGlobalStageCheck);
  layout->addRow(tr("Accept JSON payloads"), m_acceptJSONCheck);
  layout->addRow(tr("Accept CBOR payloads"), m_acceptCBORCheck);

  auto info = new QLabel(tr(
      "OpenTrackIO (SMPTE RiS-OSVP)\n\n"
      "Receives on-set virtual production metadata (camera pose, lens,\n"
      "timing/timecode, PTP sync) on UDP multicast.\n"
      "One multicast group is joined per source number:\n"
      "  239.135.1.<N>:55555 for each N in [min..max].\n\n"
      "Payload: JSON (or CBOR) behind a 16-byte OTrk header.\n"
      "Reference: https://www.opentrackio.org/"));
  info->setWordWrap(true);
  layout->addRow(info);
  setLayout(layout);
}

Device::DeviceSettings OpenTrackIOProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = OpenTrackIOProtocolFactory::static_concreteKey();

  OpenTrackIOSpecificSettings spec;
  spec.multicastBaseAddress = m_multicastBaseEdit->text();
  spec.port = m_portEdit->value();
  spec.minSourceNumber = m_minSourceEdit->value();
  spec.maxSourceNumber = m_maxSourceEdit->value();
  spec.enableCamera = m_enableCameraCheck->isChecked();
  spec.enableLens = m_enableLensCheck->isChecked();
  spec.enableTiming = m_enableTimingCheck->isChecked();
  spec.enableGlobalStage = m_enableGlobalStageCheck->isChecked();
  spec.acceptJSON = m_acceptJSONCheck->isChecked();
  spec.acceptCBOR = m_acceptCBORCheck->isChecked();

  s.deviceSpecificSettings = QVariant::fromValue(spec);
  return s;
}

void OpenTrackIOProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  if(settings.deviceSpecificSettings.canConvert<OpenTrackIOSpecificSettings>())
  {
    auto spec = settings.deviceSpecificSettings.value<OpenTrackIOSpecificSettings>();
    m_multicastBaseEdit->setText(spec.multicastBaseAddress);
    m_portEdit->setValue(spec.port);
    m_minSourceEdit->setValue(spec.minSourceNumber);
    m_maxSourceEdit->setValue(spec.maxSourceNumber);
    m_enableCameraCheck->setChecked(spec.enableCamera);
    m_enableLensCheck->setChecked(spec.enableLens);
    m_enableTimingCheck->setChecked(spec.enableTiming);
    m_enableGlobalStageCheck->setChecked(spec.enableGlobalStage);
    m_acceptJSONCheck->setChecked(spec.acceptJSON);
    m_acceptCBORCheck->setChecked(spec.acceptCBOR);
  }
}

}
