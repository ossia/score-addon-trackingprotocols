# Tests for the tracking protocols.
#
# The parsers are compiled straight into the tests rather than linked from the
# plug-in: score plug-ins are built with hidden visibility, so nothing in them
# is reachable from a test, and the parsers only need ossia anyway.
#
# Reference material:
#   PSN          3rdparty/psn-cpp (VYV's official library: its encoder builds the packets)
#   RTTrP        https://rttrp.github.io/RTTrP-Wiki/RTTrPM.html + 3rdparty/RTTrP decoders
#   TUIO 1.1/2.0 https://www.tuio.org/?specification, https://www.tuio.org/?tuio20
#   OpenTrackIO  https://ris-pub.smpte.org/ris-osvp-metadata-camdkit/ (transport spec,
#                published examples in data/opentrackio/, Mo-Sys opentrackio-cpp parser)

set(TRACKING_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(TRACKING_TEST_DATA "${CMAKE_CURRENT_SOURCE_DIR}/data")

# The slot manager that every protocol maps transient ids onto stable tree slots with.
score_add_test(test_tracking_slot_manager
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/TrackingSlotManagerTest.cpp")
target_include_directories(test_tracking_slot_manager PRIVATE "${TRACKING_ROOT}")

# RTTrPM wire format, against the protocol definition and the reference decoders.
score_add_test(test_tracking_rttrp
  SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/RTTrPParserTest.cpp"
    "${TRACKING_ROOT}/RTTrP/RTTrPParser.cpp"
    "${TRACKING_ROOT}/RTTrP/RTTrPProtocol.cpp")
target_include_directories(test_tracking_rttrp PRIVATE "${TRACKING_ROOT}")

# PosiStageNet, with packets produced by the official encoder.
score_add_test(test_tracking_psn
  SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/PSNTest.cpp"
    "${TRACKING_ROOT}/PSN/PSNProtocol.cpp")
target_include_directories(test_tracking_psn PRIVATE
  "${TRACKING_ROOT}"
  "${TRACKING_ROOT}/3rdparty/psn-cpp/include")

# TUIO 1.1 and 2.0 messages as the specification writes them.
score_add_test(test_tracking_tuio
  SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/TUIOTest.cpp"
    "${TRACKING_ROOT}/TUIO/TUIOProtocol.cpp"
    "${TRACKING_ROOT}/TUIO/TUIO2Handlers.cpp")
target_include_directories(test_tracking_tuio PRIVATE "${TRACKING_ROOT}")

# OpenTrackIO transport header + the published example samples.
score_add_test(test_tracking_opentrackio
  SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenTrackIOTest.cpp"
    "${TRACKING_ROOT}/OpenTrackIO/OpenTrackIOProtocol.cpp"
    "${TRACKING_ROOT}/3rdparty/opentrackio-cpp/src/OpenTrackIOProperties.cpp"
    "${TRACKING_ROOT}/3rdparty/opentrackio-cpp/src/OpenTrackIOSample.cpp")
target_include_directories(test_tracking_opentrackio PRIVATE
  "${TRACKING_ROOT}"
  "${TRACKING_ROOT}/3rdparty/opentrackio-cpp/include"
  "${TRACKING_ROOT}/3rdparty/opentrackio-cpp/external")
target_compile_features(test_tracking_opentrackio PRIVATE cxx_std_20)
target_compile_definitions(test_tracking_opentrackio PRIVATE
  TRACKING_TEST_DATA="${TRACKING_TEST_DATA}")

# The devices themselves (can they be added to a document, do they coexist)
# are tested from the score tree, in tests/unit/TrackingDevicesTest.cpp: the
# app fixtures they need are only available there.
