#pragma once

#include <QString>
#include <verdigris>

namespace OpenXR
{

struct OpenXRSpecificSettings
{
  bool enableHandTracking{true};
  bool enableControllers{true};
  int pollRateMs{11}; // ~90Hz
};

}

Q_DECLARE_METATYPE(OpenXR::OpenXRSpecificSettings)
W_REGISTER_ARGTYPE(OpenXR::OpenXRSpecificSettings)
