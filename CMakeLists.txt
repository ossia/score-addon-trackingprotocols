project(score_addon_trackingprotocols LANGUAGES CXX)

return()
# Find OpenXR early so we can conditionally include files
find_package(OpenXR QUIET)

set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/score_addon_trackingprotocols.hpp"

  # Common infrastructure
  "${CMAKE_CURRENT_SOURCE_DIR}/Common/TrackingTypes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Common/TrackingSlotManager.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Common/TrackingTreeBuilder.hpp"

  # TUIO protocol
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIODevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOProtocol.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIO2Types.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOSpecificSettings.hpp"

  # PSN protocol
  "${CMAKE_CURRENT_SOURCE_DIR}/PSN/PSNDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/PSN/PSNProtocol.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/PSN/PSNProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/PSN/PSNProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/PSN/PSNSpecificSettings.hpp"

  # RTTrP protocol
  "${CMAKE_CURRENT_SOURCE_DIR}/RTTrP/RTTrPDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RTTrP/RTTrPProtocol.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RTTrP/RTTrPProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RTTrP/RTTrPProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RTTrP/RTTrPSpecificSettings.hpp"

  # OpenTrackIO protocol (SMPTE RiS-OSVP)
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenTrackIO/OpenTrackIODevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenTrackIO/OpenTrackIOProtocol.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenTrackIO/OpenTrackIOProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenTrackIO/OpenTrackIOProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenTrackIO/OpenTrackIOSpecificSettings.hpp"

)

set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/score_addon_trackingprotocols.cpp"

  # TUIO protocol
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIODevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOProtocol.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIO2Handlers.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOSpecificSettingsSerialization.cpp"

  # PSN protocol
  "${CMAKE_CURRENT_SOURCE_DIR}/PSN/PSNDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/PSN/PSNProtocol.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/PSN/PSNProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/PSN/PSNProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/PSN/PSNSpecificSettingsSerialization.cpp"

  # RTTrP protocol
  "${CMAKE_CURRENT_SOURCE_DIR}/RTTrP/RTTrPDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RTTrP/RTTrPProtocol.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RTTrP/RTTrPProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RTTrP/RTTrPProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/RTTrP/RTTrPSpecificSettingsSerialization.cpp"

  # OpenTrackIO protocol (SMPTE RiS-OSVP)
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenTrackIO/OpenTrackIODevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenTrackIO/OpenTrackIOProtocol.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenTrackIO/OpenTrackIOProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenTrackIO/OpenTrackIOProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenTrackIO/OpenTrackIOSpecificSettingsSerialization.cpp"

  # Mo-Sys opentrackio-cpp parser (vendored)
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/opentrackio-cpp/src/OpenTrackIOProperties.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/opentrackio-cpp/src/OpenTrackIOSample.cpp"

)

# Creation of the library
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

# Include directories for 3rdparty libraries
target_include_directories(${PROJECT_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/psn-cpp/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/tsopenxr"
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/opentrackio-cpp/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/opentrackio-cpp/external"
)

# opentrackio-cpp (and by extension nlohmann/json.hpp) requires C++20+.
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# Link
target_link_libraries(${PROJECT_NAME} PUBLIC
  score_plugin_deviceexplorer
)

# Add OpenXR support if available
if(OpenXR_FOUND)
  target_sources(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenXR/OpenXRDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenXR/OpenXRProtocol.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenXR/OpenXRProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenXR/OpenXRProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenXR/OpenXRSpecificSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenXR/OpenXRProtocol.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenXR/OpenXRProtocolFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenXR/OpenXRProtocolSettingsWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenXR/OpenXRSpecificSettingsSerialization.cpp"
  )
  target_link_libraries(${PROJECT_NAME} PUBLIC
    Qt6::OpenGL
    OpenXR::openxr_loader
  )
  target_compile_definitions(${PROJECT_NAME} PRIVATE SCORE_HAS_OPENXR=1)

  # Linux needs X11 for GLX context access
  if(UNIX AND NOT APPLE)
    find_package(X11)
    if(X11_FOUND)
      target_link_libraries(${PROJECT_NAME} PRIVATE X11::X11)
    endif()
  endif()

  message(STATUS "OpenXR support enabled for score-addon-trackingprotocols")
else()
  message(STATUS "OpenXR not found - OpenXR protocol disabled")
endif()

# Target-specific options
setup_score_plugin(${PROJECT_NAME})
