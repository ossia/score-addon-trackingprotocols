#pragma once

#include <QString>
#include <verdigris>

namespace PSN
{

struct PSNSpecificSettings
{
  QString multicastAddress{"236.10.10.10"};
  uint16_t port{56565};
  int numTrackers{16};
  bool enableVelocity{true};
  bool enableAcceleration{true};
  bool enableOrientation{true};
  bool enableTargetPosition{false};
};

}

Q_DECLARE_METATYPE(PSN::PSNSpecificSettings)
W_REGISTER_ARGTYPE(PSN::PSNSpecificSettings)
