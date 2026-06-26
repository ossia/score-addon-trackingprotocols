#pragma once

#include "OpenXRSpecificSettings.hpp"
#include "../Common/TrackingTypes.hpp"

#include <ossia/network/base/protocol.hpp>
#include <ossia/network/context.hpp>

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QTimer>

#include <array>
#include <memory>

// Forward declarations for OpenXR types
typedef uint64_t XrSystemId;
struct XrInstance_T;
typedef struct XrInstance_T* XrInstance;
struct XrSession_T;
typedef struct XrSession_T* XrSession;
struct XrSpace_T;
typedef struct XrSpace_T* XrSpace;
struct XrAction_T;
typedef struct XrAction_T* XrAction;
struct XrActionSet_T;
typedef struct XrActionSet_T* XrActionSet;
struct XrHandTrackerEXT_T;
typedef struct XrHandTrackerEXT_T* XrHandTrackerEXT;
typedef uint64_t XrPath;
typedef int64_t XrTime;

namespace OpenXR
{

// Hand joint data
struct HandJointData
{
  std::array<Tracking::Position3D, 26> positions;
  std::array<Tracking::Quaternion, 26> orientations;
  std::array<float, 26> radii;
  std::array<bool, 26> valid;
  bool tracked{false};
};

// Controller data
struct ControllerData
{
  Tracking::Position3D position;
  Tracking::Quaternion orientation;
  float grab{0.0f};
  float trigger{0.0f};
  bool active{false};
};

class OpenXRProtocol final : public ossia::net::protocol_base
{
public:
  explicit OpenXRProtocol(
      const ossia::net::network_context_ptr& ctx,
      OpenXRSpecificSettings settings);

  ~OpenXRProtocol();

  void set_device(ossia::net::device_base& dev) override;

  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value&) override { return false; }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base& node_base) override { return false; }

private:
  bool initializeOpenGL();
  bool initializeOpenXR();
  bool initializeHandTracking();
  void createDeviceTree(ossia::net::node_base& root);
  void shutdown();

  void poll();
  void pollControllers();
  void pollHandTracking();

  void updateControllerParams(int hand, const ControllerData& data);
  void updateHandParams(int hand, const HandJointData& data);

  ossia::net::network_context_ptr m_ctx;
  ossia::net::device_base* m_device{nullptr};

  // Qt OpenGL context
  std::unique_ptr<QOffscreenSurface> m_surface;
  std::unique_ptr<QOpenGLContext> m_glContext;

  // OpenXR handles
  XrInstance m_xrInstance{nullptr};
  XrSystemId m_xrSystemId{0};
  XrSession m_xrSession{nullptr};
  XrSpace m_xrSpace{nullptr};
  XrActionSet m_xrActionSet{nullptr};
  XrAction m_poseAction{nullptr};
  XrAction m_grabAction{nullptr};
  XrAction m_triggerAction{nullptr};
  XrPath m_handPaths[2]{0, 0};
  XrSpace m_handSpaces[2]{nullptr, nullptr};

  // Session state
  int m_xrSessionState{0};
  bool m_sessionRunning{false};
  XrTime m_predictedDisplayTime{0};

  // Hand trackers (XR_EXT_hand_tracking)
  XrHandTrackerEXT m_handTrackers[2]{nullptr, nullptr};
  bool m_handTrackingSupported{false};

  // Cached data
  std::array<ControllerData, 2> m_controllers;
  std::array<HandJointData, 2> m_hands;

  // Polling timer
  std::unique_ptr<QTimer> m_pollTimer;

  OpenXRSpecificSettings m_settings;
  bool m_initialized{false};

  // Function pointers for hand tracking extension
  void* m_xrCreateHandTrackerEXT{nullptr};
  void* m_xrDestroyHandTrackerEXT{nullptr};
  void* m_xrLocateHandJointsEXT{nullptr};
};

}
