#pragma once

#include <QString>
#include <verdigris>

namespace TUIO
{
struct TUIOSpecificSettings
{
  uint16_t port{3333};
  int numObjects{8};
  int numCursors{8};
  int numBlobs{8};
};
}

Q_DECLARE_METATYPE(TUIO::TUIOSpecificSettings)
W_REGISTER_ARGTYPE(TUIO::TUIOSpecificSettings)