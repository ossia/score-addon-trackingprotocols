project(score_addon_trackingprotocols LANGUAGES CXX)


set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/score_addon_trackingprotocols.hpp"
  
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIODevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOProtocol.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOSpecificSettings.hpp"
)

set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/score_addon_trackingprotocols.cpp"
  
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIODevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOProtocol.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/TUIO/TUIOSpecificSettingsSerialization.cpp"
)

# Creation of the library
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

# Link
target_link_libraries(${PROJECT_NAME} PUBLIC 
  score_plugin_deviceexplorer
)

# Target-specific options
setup_score_plugin(${PROJECT_NAME})
