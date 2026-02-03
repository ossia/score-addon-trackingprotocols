project(score_addon_trackingprotocols LANGUAGES CXX)


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
)

# Creation of the library
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

# Include directories for 3rdparty libraries
target_include_directories(${PROJECT_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/psn-cpp/include"
)

# Link
target_link_libraries(${PROJECT_NAME} PUBLIC
  score_plugin_deviceexplorer
)

# Target-specific options
setup_score_plugin(${PROJECT_NAME})
