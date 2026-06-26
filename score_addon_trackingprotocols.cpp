#include "score_addon_trackingprotocols.hpp"

#include <score/plugins/FactorySetup.hpp>

#include "TUIO/TUIOProtocolFactory.hpp"
#include "PSN/PSNProtocolFactory.hpp"
#include "RTTrP/RTTrPProtocolFactory.hpp"
#include "OpenTrackIO/OpenTrackIOProtocolFactory.hpp"
#if defined(SCORE_HAS_OPENXR)
#include "OpenXR/OpenXRProtocolFactory.hpp"
#endif

score_addon_trackingprotocols::score_addon_trackingprotocols() = default;
score_addon_trackingprotocols::~score_addon_trackingprotocols() = default;

std::vector<score::InterfaceBase*> score_addon_trackingprotocols::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Device::ProtocolFactory,
         TUIO::TUIOProtocolFactory,
         PSN::PSNProtocolFactory,
         RTTrP::RTTrPProtocolFactory,
         OpenTrackIO::OpenTrackIOProtocolFactory
#if defined(SCORE_HAS_OPENXR)
         , OpenXR::OpenXRProtocolFactory
#endif
         >
      >(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_addon_trackingprotocols)
