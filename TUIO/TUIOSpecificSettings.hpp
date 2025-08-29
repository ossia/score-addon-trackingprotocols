#pragma once

#include <QString>
#include <verdigris>

namespace TUIO
{
struct TUIOSpecificSettings
{
  uint16_t port{3333};
};
}

Q_DECLARE_METATYPE(TUIO::TUIOSpecificSettings)
W_REGISTER_ARGTYPE(TUIO::TUIOSpecificSettings)