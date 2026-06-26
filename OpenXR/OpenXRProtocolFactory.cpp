#include "OpenXRProtocolFactory.hpp"
#include "OpenXRProtocolSettingsWidget.hpp"

namespace OpenXR
{

Device::ProtocolSettingsWidget* OpenXRProtocolFactory::makeSettingsWidget()
{
  return new OpenXRProtocolSettingsWidget;
}

}
