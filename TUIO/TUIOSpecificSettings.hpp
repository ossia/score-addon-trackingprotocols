#pragma once

#include <QString>
#include <verdigris>

namespace TUIO
{
enum class TUIOVersion
{
  V1_1,
  V2_0
};

struct TUIOSpecificSettings
{
  uint16_t port{3333};
  int numObjects{8};
  int numCursors{8};
  int numBlobs{8};
  TUIOVersion version{TUIOVersion::V1_1};
};
}

Q_DECLARE_METATYPE(TUIO::TUIOSpecificSettings)
W_REGISTER_ARGTYPE(TUIO::TUIOSpecificSettings)