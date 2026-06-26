#pragma once

#include <QString>
#include <verdigris>

namespace OpenTrackIO
{

struct OpenTrackIOSpecificSettings
{
  // Multicast base prefix; each listened source N joins 239.135.1.N (per spec).
  QString multicastBaseAddress{"239.135.1"};
  uint16_t port{55555};

  // Source number range (1-200 per spec). For single-source operation, set
  // min==max. One UDP socket is created per source number, each joining its
  // own IGMP group.
  int minSourceNumber{1};
  int maxSourceNumber{1};

  bool enableCamera{true};
  bool enableLens{true};
  bool enableTiming{true};
  bool enableGlobalStage{false};

  bool acceptJSON{true};
  bool acceptCBOR{false};
};

}

Q_DECLARE_METATYPE(OpenTrackIO::OpenTrackIOSpecificSettings)
W_REGISTER_ARGTYPE(OpenTrackIO::OpenTrackIOSpecificSettings)
