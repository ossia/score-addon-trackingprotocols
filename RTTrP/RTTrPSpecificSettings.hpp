#pragma once

#include <QString>
#include <verdigris>

namespace RTTrP
{

struct RTTrPSpecificSettings
{
  uint16_t port{24002};
  int numTrackables{16};
  int maxLEDsPerTrackable{8};
  int maxZones{8};
  bool enableQuaternion{true};
  bool enableEuler{false};
  bool enableVelocity{true};
  bool enableAcceleration{true};
  bool enableZones{true};
};

}

Q_DECLARE_METATYPE(RTTrP::RTTrPSpecificSettings)
W_REGISTER_ARGTYPE(RTTrP::RTTrPSpecificSettings)
