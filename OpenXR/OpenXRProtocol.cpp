#include "OpenXRProtocol.hpp"
#include "../Common/TrackingTreeBuilder.hpp"

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>

#include <QCoreApplication>
#include <QThread>

// Platform-specific includes and OpenXR defines
#if defined(_WIN32)
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#include <Windows.h>
#elif defined(__linux__) && !defined(__ANDROID__)
#define XR_USE_PLATFORM_XLIB
#define XR_USE_GRAPHICS_API_OPENGL
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#endif

// OpenXR headers (after platform defines)
#include "../3rdparty/tsopenxr/openxr/openxr.h"
#include "../3rdparty/tsopenxr/openxr/openxr_platform.h"

namespace OpenXR
{

// Joint names for the device tree
static const char* jointNames[26] = {
    "palm", "wrist",
    "thumb_metacarpal", "thumb_proximal", "thumb_distal", "thumb_tip",
    "index_metacarpal", "index_proximal", "index_intermediate", "index_distal", "index_tip",
    "middle_metacarpal", "middle_proximal", "middle_intermediate", "middle_distal", "middle_tip",
    "ring_metacarpal", "ring_proximal", "ring_intermediate", "ring_distal", "ring_tip",
    "little_metacarpal", "little_proximal", "little_intermediate", "little_distal", "little_tip"
};

OpenXRProtocol::OpenXRProtocol(
    const ossia::net::network_context_ptr& ctx,
    OpenXRSpecificSettings settings)
    : protocol_base{flags{}}
    , m_ctx{ctx}
    , m_settings{std::move(settings)}
{
}

OpenXRProtocol::~OpenXRProtocol()
{
  shutdown();
}

void OpenXRProtocol::set_device(ossia::net::device_base& dev)
{
  m_device = &dev;

  if (!initializeOpenGL())
  {
    ossia::logger().error("OpenXR: Failed to initialize OpenGL context");
    return;
  }

  if (!initializeOpenXR())
  {
    ossia::logger().error("OpenXR: Failed to initialize OpenXR");
    return;
  }

  if (m_settings.enableHandTracking)
  {
    if (!initializeHandTracking())
    {
      ossia::logger().warn("OpenXR: Hand tracking not available");
    }
  }

  createDeviceTree(dev.get_root_node());

  // Start polling timer
  m_pollTimer = std::make_unique<QTimer>();
  m_pollTimer->setInterval(m_settings.pollRateMs);
  QObject::connect(m_pollTimer.get(), &QTimer::timeout, [this]() { poll(); });
  m_pollTimer->start();

  m_initialized = true;
}

bool OpenXRProtocol::initializeOpenGL()
{
  // Create offscreen surface
  m_surface = std::make_unique<QOffscreenSurface>();

  QSurfaceFormat format;
  format.setMajorVersion(4);
  format.setMinorVersion(5);
  format.setProfile(QSurfaceFormat::CoreProfile);
  m_surface->setFormat(format);
  m_surface->create();

  if (!m_surface->isValid())
  {
    ossia::logger().error("OpenXR: Failed to create offscreen surface");
    return false;
  }

  // Create OpenGL context
  m_glContext = std::make_unique<QOpenGLContext>();
  m_glContext->setFormat(format);

  if (!m_glContext->create())
  {
    ossia::logger().error("OpenXR: Failed to create OpenGL context");
    return false;
  }

  // Make context current
  if (!m_glContext->makeCurrent(m_surface.get()))
  {
    ossia::logger().error("OpenXR: Failed to make OpenGL context current");
    return false;
  }

  ossia::logger().info("OpenXR: OpenGL context initialized (version {}.{})",
                       m_glContext->format().majorVersion(),
                       m_glContext->format().minorVersion());

  return true;
}

bool OpenXRProtocol::initializeOpenXR()
{
  // Make sure GL context is current
  m_glContext->makeCurrent(m_surface.get());

  XrResult result;

  // Enumerate extensions
  uint32_t extensionCount = 0;
  result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);
  if (XR_FAILED(result))
  {
    ossia::logger().error("OpenXR: Failed to enumerate extensions");
    return false;
  }

  std::vector<XrExtensionProperties> extensions(extensionCount);
  for (auto& ext : extensions)
  {
    ext.type = XR_TYPE_EXTENSION_PROPERTIES;
    ext.next = nullptr;
  }
  result = xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensions.data());
  if (XR_FAILED(result))
  {
    ossia::logger().error("OpenXR: Failed to get extensions");
    return false;
  }

  // Check for required extensions
  bool hasOpenGL = false;
  bool hasHandTracking = false;
  for (const auto& ext : extensions)
  {
#if defined(_WIN32)
    if (std::strcmp(ext.extensionName, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME) == 0)
      hasOpenGL = true;
#elif defined(__linux__)
    if (std::strcmp(ext.extensionName, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME) == 0)
      hasOpenGL = true;
#endif
    if (std::strcmp(ext.extensionName, XR_EXT_HAND_TRACKING_EXTENSION_NAME) == 0)
      hasHandTracking = true;
  }

  if (!hasOpenGL)
  {
    ossia::logger().error("OpenXR: OpenGL extension not supported");
    return false;
  }

  m_handTrackingSupported = hasHandTracking;

  // Create instance
  std::vector<const char*> enabledExtensions;
  enabledExtensions.push_back(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
  if (hasHandTracking && m_settings.enableHandTracking)
  {
    enabledExtensions.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
  }

  XrInstanceCreateInfo instanceCreateInfo{};
  instanceCreateInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.next = nullptr;
  std::strcpy(instanceCreateInfo.applicationInfo.applicationName, "ossia score");
  instanceCreateInfo.applicationInfo.applicationVersion = 1;
  std::strcpy(instanceCreateInfo.applicationInfo.engineName, "ossia");
  instanceCreateInfo.applicationInfo.engineVersion = 1;
  instanceCreateInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
  instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
  instanceCreateInfo.enabledExtensionNames = enabledExtensions.data();

  result = xrCreateInstance(&instanceCreateInfo, &m_xrInstance);
  if (XR_FAILED(result))
  {
    ossia::logger().error("OpenXR: Failed to create instance");
    return false;
  }

  // Get system
  XrSystemGetInfo systemGetInfo{};
  systemGetInfo.type = XR_TYPE_SYSTEM_GET_INFO;
  systemGetInfo.next = nullptr;
  systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  result = xrGetSystem(m_xrInstance, &systemGetInfo, &m_xrSystemId);
  if (XR_FAILED(result))
  {
    ossia::logger().error("OpenXR: Failed to get system (is HMD connected?)");
    return false;
  }

  // Check graphics requirements
  PFN_xrGetOpenGLGraphicsRequirementsKHR xrGetOpenGLGraphicsRequirementsKHR = nullptr;
  result = xrGetInstanceProcAddr(m_xrInstance, "xrGetOpenGLGraphicsRequirementsKHR",
                                  (PFN_xrVoidFunction*)&xrGetOpenGLGraphicsRequirementsKHR);
  if (XR_FAILED(result) || !xrGetOpenGLGraphicsRequirementsKHR)
  {
    ossia::logger().error("OpenXR: Failed to get OpenGL requirements function");
    return false;
  }

  XrGraphicsRequirementsOpenGLKHR graphicsRequirements{};
  graphicsRequirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
  graphicsRequirements.next = nullptr;
  result = xrGetOpenGLGraphicsRequirementsKHR(m_xrInstance, m_xrSystemId, &graphicsRequirements);
  if (XR_FAILED(result))
  {
    ossia::logger().error("OpenXR: Failed to get graphics requirements");
    return false;
  }

  // Create session with platform-specific graphics binding
#if defined(_WIN32)
  XrGraphicsBindingOpenGLWin32KHR graphicsBinding{};
  graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
  graphicsBinding.next = nullptr;
  graphicsBinding.hDC = wglGetCurrentDC();
  graphicsBinding.hGLRC = wglGetCurrentContext();
#elif defined(__linux__) && !defined(__ANDROID__)
  // Get native handles from Qt
  auto* nativeInterface = QGuiApplication::platformNativeInterface();
  Display* xDisplay = static_cast<Display*>(nativeInterface->nativeResourceForContext("display", m_glContext.get()));
  GLXContext glxContext = static_cast<GLXContext>(nativeInterface->nativeResourceForContext("glxcontext", m_glContext.get()));
  GLXDrawable glxDrawable = static_cast<GLXDrawable>(reinterpret_cast<uintptr_t>(
      nativeInterface->nativeResourceForContext("glxconfig", m_glContext.get())));

  // Get visual info
  int screen = DefaultScreen(xDisplay);
  XVisualInfo visualInfoTemplate;
  visualInfoTemplate.screen = screen;
  int numVisuals = 0;
  XVisualInfo* visualInfo = XGetVisualInfo(xDisplay, VisualScreenMask, &visualInfoTemplate, &numVisuals);
  uint32_t visualId = visualInfo ? visualInfo->visualid : 0;
  if (visualInfo) XFree(visualInfo);

  // Get GLX FB config
  int numConfigs = 0;
  GLXFBConfig* fbConfigs = glXGetFBConfigs(xDisplay, screen, &numConfigs);
  GLXFBConfig fbConfig = (numConfigs > 0) ? fbConfigs[0] : nullptr;
  if (fbConfigs) XFree(fbConfigs);

  XrGraphicsBindingOpenGLXlibKHR graphicsBinding{};
  graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
  graphicsBinding.next = nullptr;
  graphicsBinding.xDisplay = xDisplay;
  graphicsBinding.visualid = visualId;
  graphicsBinding.glxFBConfig = fbConfig;
  graphicsBinding.glxDrawable = glxDrawable ? glxDrawable : glXGetCurrentDrawable();
  graphicsBinding.glxContext = glxContext ? glxContext : glXGetCurrentContext();
#else
  ossia::logger().error("OpenXR: Platform not supported");
  return false;
#endif

  XrSessionCreateInfo sessionCreateInfo{};
  sessionCreateInfo.type = XR_TYPE_SESSION_CREATE_INFO;
  sessionCreateInfo.next = &graphicsBinding;
  sessionCreateInfo.systemId = m_xrSystemId;

  result = xrCreateSession(m_xrInstance, &sessionCreateInfo, &m_xrSession);
  if (XR_FAILED(result))
  {
    ossia::logger().error("OpenXR: Failed to create session");
    return false;
  }

  // Create reference space
  XrReferenceSpaceCreateInfo spaceCreateInfo{};
  spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
  spaceCreateInfo.next = nullptr;
  spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  spaceCreateInfo.poseInReferenceSpace.orientation = {0, 0, 0, 1};
  spaceCreateInfo.poseInReferenceSpace.position = {0, 0, 0};

  result = xrCreateReferenceSpace(m_xrSession, &spaceCreateInfo, &m_xrSpace);
  if (XR_FAILED(result))
  {
    // Try LOCAL space if STAGE is not available
    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    result = xrCreateReferenceSpace(m_xrSession, &spaceCreateInfo, &m_xrSpace);
    if (XR_FAILED(result))
    {
      ossia::logger().error("OpenXR: Failed to create reference space");
      return false;
    }
  }

  // Create action set
  XrActionSetCreateInfo actionSetCreateInfo{};
  actionSetCreateInfo.type = XR_TYPE_ACTION_SET_CREATE_INFO;
  actionSetCreateInfo.next = nullptr;
  std::strcpy(actionSetCreateInfo.actionSetName, "ossia");
  std::strcpy(actionSetCreateInfo.localizedActionSetName, "ossia");

  result = xrCreateActionSet(m_xrInstance, &actionSetCreateInfo, &m_xrActionSet);
  if (XR_FAILED(result))
  {
    ossia::logger().error("OpenXR: Failed to create action set");
    return false;
  }

  // Create hand paths
  xrStringToPath(m_xrInstance, "/user/hand/left", &m_handPaths[0]);
  xrStringToPath(m_xrInstance, "/user/hand/right", &m_handPaths[1]);

  // Create pose action
  XrActionCreateInfo poseActionCreateInfo{};
  poseActionCreateInfo.type = XR_TYPE_ACTION_CREATE_INFO;
  poseActionCreateInfo.next = nullptr;
  poseActionCreateInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
  std::strcpy(poseActionCreateInfo.actionName, "hand_pose");
  std::strcpy(poseActionCreateInfo.localizedActionName, "Hand Pose");
  poseActionCreateInfo.countSubactionPaths = 2;
  poseActionCreateInfo.subactionPaths = m_handPaths;

  result = xrCreateAction(m_xrActionSet, &poseActionCreateInfo, &m_poseAction);
  if (XR_FAILED(result))
  {
    ossia::logger().warn("OpenXR: Failed to create pose action");
  }

  // Create grab action
  XrActionCreateInfo grabActionCreateInfo{};
  grabActionCreateInfo.type = XR_TYPE_ACTION_CREATE_INFO;
  grabActionCreateInfo.next = nullptr;
  grabActionCreateInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
  std::strcpy(grabActionCreateInfo.actionName, "grab");
  std::strcpy(grabActionCreateInfo.localizedActionName, "Grab");
  grabActionCreateInfo.countSubactionPaths = 2;
  grabActionCreateInfo.subactionPaths = m_handPaths;

  result = xrCreateAction(m_xrActionSet, &grabActionCreateInfo, &m_grabAction);
  if (XR_FAILED(result))
  {
    ossia::logger().warn("OpenXR: Failed to create grab action");
  }

  // Create trigger action
  XrActionCreateInfo triggerActionCreateInfo{};
  triggerActionCreateInfo.type = XR_TYPE_ACTION_CREATE_INFO;
  triggerActionCreateInfo.next = nullptr;
  triggerActionCreateInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
  std::strcpy(triggerActionCreateInfo.actionName, "trigger");
  std::strcpy(triggerActionCreateInfo.localizedActionName, "Trigger");
  triggerActionCreateInfo.countSubactionPaths = 2;
  triggerActionCreateInfo.subactionPaths = m_handPaths;

  result = xrCreateAction(m_xrActionSet, &triggerActionCreateInfo, &m_triggerAction);
  if (XR_FAILED(result))
  {
    ossia::logger().warn("OpenXR: Failed to create trigger action");
  }

  // Suggest bindings for Oculus Touch controller
  XrPath interactionProfilePath;
  xrStringToPath(m_xrInstance, "/interaction_profiles/oculus/touch_controller", &interactionProfilePath);

  XrPath posePaths[2], squeezePaths[2], triggerPaths[2];
  xrStringToPath(m_xrInstance, "/user/hand/left/input/grip/pose", &posePaths[0]);
  xrStringToPath(m_xrInstance, "/user/hand/right/input/grip/pose", &posePaths[1]);
  xrStringToPath(m_xrInstance, "/user/hand/left/input/squeeze/value", &squeezePaths[0]);
  xrStringToPath(m_xrInstance, "/user/hand/right/input/squeeze/value", &squeezePaths[1]);
  xrStringToPath(m_xrInstance, "/user/hand/left/input/trigger/value", &triggerPaths[0]);
  xrStringToPath(m_xrInstance, "/user/hand/right/input/trigger/value", &triggerPaths[1]);

  std::vector<XrActionSuggestedBinding> bindings = {
      {m_poseAction, posePaths[0]},
      {m_poseAction, posePaths[1]},
      {m_grabAction, squeezePaths[0]},
      {m_grabAction, squeezePaths[1]},
      {m_triggerAction, triggerPaths[0]},
      {m_triggerAction, triggerPaths[1]},
  };

  XrInteractionProfileSuggestedBinding suggestedBindings{};
  suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
  suggestedBindings.next = nullptr;
  suggestedBindings.interactionProfile = interactionProfilePath;
  suggestedBindings.suggestedBindings = bindings.data();
  suggestedBindings.countSuggestedBindings = static_cast<uint32_t>(bindings.size());

  xrSuggestInteractionProfileBindings(m_xrInstance, &suggestedBindings);

  // Create action spaces for hand poses
  for (int hand = 0; hand < 2; ++hand)
  {
    XrActionSpaceCreateInfo actionSpaceCreateInfo{};
    actionSpaceCreateInfo.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
    actionSpaceCreateInfo.next = nullptr;
    actionSpaceCreateInfo.action = m_poseAction;
    actionSpaceCreateInfo.subactionPath = m_handPaths[hand];
    actionSpaceCreateInfo.poseInActionSpace.orientation = {0, 0, 0, 1};
    actionSpaceCreateInfo.poseInActionSpace.position = {0, 0, 0};

    result = xrCreateActionSpace(m_xrSession, &actionSpaceCreateInfo, &m_handSpaces[hand]);
    if (XR_FAILED(result))
    {
      ossia::logger().warn("OpenXR: Failed to create hand space for hand {}", hand);
    }
  }

  // Attach action set to session
  XrSessionActionSetsAttachInfo attachInfo{};
  attachInfo.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
  attachInfo.next = nullptr;
  attachInfo.countActionSets = 1;
  attachInfo.actionSets = &m_xrActionSet;

  result = xrAttachSessionActionSets(m_xrSession, &attachInfo);
  if (XR_FAILED(result))
  {
    ossia::logger().warn("OpenXR: Failed to attach action sets");
  }

  ossia::logger().info("OpenXR: Initialized successfully");
  return true;
}

bool OpenXRProtocol::initializeHandTracking()
{
  if (!m_handTrackingSupported)
    return false;

  // Get function pointers
  XrResult result = xrGetInstanceProcAddr(
      m_xrInstance, "xrCreateHandTrackerEXT",
      (PFN_xrVoidFunction*)&m_xrCreateHandTrackerEXT);
  if (XR_FAILED(result))
    return false;

  result = xrGetInstanceProcAddr(
      m_xrInstance, "xrDestroyHandTrackerEXT",
      (PFN_xrVoidFunction*)&m_xrDestroyHandTrackerEXT);
  if (XR_FAILED(result))
    return false;

  result = xrGetInstanceProcAddr(
      m_xrInstance, "xrLocateHandJointsEXT",
      (PFN_xrVoidFunction*)&m_xrLocateHandJointsEXT);
  if (XR_FAILED(result))
    return false;

  // Create hand trackers
  auto createHandTracker = (PFN_xrCreateHandTrackerEXT)m_xrCreateHandTrackerEXT;

  for (int hand = 0; hand < 2; ++hand)
  {
    XrHandTrackerCreateInfoEXT createInfo{};
    createInfo.type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT;
    createInfo.next = nullptr;
    createInfo.hand = (hand == 0) ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT;
    createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;

    result = createHandTracker(m_xrSession, &createInfo, &m_handTrackers[hand]);
    if (XR_FAILED(result))
    {
      ossia::logger().warn("OpenXR: Failed to create hand tracker for hand {}", hand);
      m_handTrackers[hand] = nullptr;
    }
  }

  bool success = (m_handTrackers[0] != nullptr || m_handTrackers[1] != nullptr);
  if (success)
  {
    ossia::logger().info("OpenXR: Hand tracking initialized");
  }

  return success;
}

void OpenXRProtocol::createDeviceTree(ossia::net::node_base& root)
{
  using TB = Tracking::TreeBuilder;

  // Frame info
  TB::create_int_param(root, "frame");
  TB::create_bool_param(root, "session_active");

  // Controllers
  if (m_settings.enableControllers)
  {
    auto& controllers = ossia::net::find_or_create_node(root, "controllers");
    for (int hand = 0; hand < 2; ++hand)
    {
      const char* name = (hand == 0) ? "left" : "right";
      auto& handNode = ossia::net::find_or_create_node(controllers, name);

      TB::create_bool_param(handNode, "active");
      TB::create_vec3f_param(handNode, "position");
      TB::create_vec4f_param(handNode, "orientation");
      TB::create_float_param(handNode, "grab");
      TB::create_float_param(handNode, "trigger");
    }
  }

  // Hands (skeleton tracking)
  if (m_settings.enableHandTracking && m_handTrackingSupported)
  {
    auto& hands = ossia::net::find_or_create_node(root, "hands");
    for (int hand = 0; hand < 2; ++hand)
    {
      const char* name = (hand == 0) ? "left" : "right";
      auto& handNode = ossia::net::find_or_create_node(hands, name);

      TB::create_bool_param(handNode, "tracked");

      auto& jointsNode = ossia::net::find_or_create_node(handNode, "joints");
      for (int j = 0; j < 26; ++j)
      {
        auto& jointNode = ossia::net::find_or_create_node(jointsNode, jointNames[j]);
        TB::create_bool_param(jointNode, "valid");
        TB::create_vec3f_param(jointNode, "position");
        TB::create_vec4f_param(jointNode, "orientation");
        TB::create_float_param(jointNode, "radius");
      }
    }
  }
}

void OpenXRProtocol::shutdown()
{
  if (m_pollTimer)
  {
    m_pollTimer->stop();
    m_pollTimer.reset();
  }

  // Destroy hand trackers
  if (m_xrDestroyHandTrackerEXT)
  {
    auto destroyHandTracker = (PFN_xrDestroyHandTrackerEXT)m_xrDestroyHandTrackerEXT;
    for (int hand = 0; hand < 2; ++hand)
    {
      if (m_handTrackers[hand])
      {
        destroyHandTracker(m_handTrackers[hand]);
        m_handTrackers[hand] = nullptr;
      }
    }
  }

  // Destroy hand spaces
  for (int hand = 0; hand < 2; ++hand)
  {
    if (m_handSpaces[hand])
    {
      xrDestroySpace(m_handSpaces[hand]);
      m_handSpaces[hand] = XR_NULL_HANDLE;
    }
  }

  // Destroy reference space
  if (m_xrSpace)
  {
    xrDestroySpace(m_xrSpace);
    m_xrSpace = XR_NULL_HANDLE;
  }

  // Destroy actions
  if (m_poseAction)
  {
    xrDestroyAction(m_poseAction);
    m_poseAction = XR_NULL_HANDLE;
  }
  if (m_grabAction)
  {
    xrDestroyAction(m_grabAction);
    m_grabAction = XR_NULL_HANDLE;
  }
  if (m_triggerAction)
  {
    xrDestroyAction(m_triggerAction);
    m_triggerAction = XR_NULL_HANDLE;
  }

  // Destroy action set
  if (m_xrActionSet)
  {
    xrDestroyActionSet(m_xrActionSet);
    m_xrActionSet = XR_NULL_HANDLE;
  }

  // Destroy session
  if (m_xrSession)
  {
    xrDestroySession(m_xrSession);
    m_xrSession = XR_NULL_HANDLE;
  }

  // Destroy instance
  if (m_xrInstance)
  {
    xrDestroyInstance(m_xrInstance);
    m_xrInstance = XR_NULL_HANDLE;
  }

  // Release GL context
  if (m_glContext && m_surface)
  {
    m_glContext->doneCurrent();
  }

  m_glContext.reset();
  m_surface.reset();
  m_initialized = false;
}

void OpenXRProtocol::poll()
{
  if (!m_initialized || !m_xrInstance)
    return;

  // Make GL context current for OpenXR calls
  m_glContext->makeCurrent(m_surface.get());

  // Poll events
  XrEventDataBuffer event{};
  event.type = XR_TYPE_EVENT_DATA_BUFFER;
  while (xrPollEvent(m_xrInstance, &event) == XR_SUCCESS)
  {
    switch (event.type)
    {
      case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
      {
        auto* stateEvent = reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
        m_xrSessionState = stateEvent->state;

        if (m_xrSessionState == XR_SESSION_STATE_READY)
        {
          XrSessionBeginInfo beginInfo{};
          beginInfo.type = XR_TYPE_SESSION_BEGIN_INFO;
          beginInfo.next = nullptr;
          beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
          xrBeginSession(m_xrSession, &beginInfo);
          m_sessionRunning = true;
        }
        else if (m_xrSessionState == XR_SESSION_STATE_STOPPING)
        {
          xrEndSession(m_xrSession);
          m_sessionRunning = false;
        }
        break;
      }
      default:
        break;
    }
    event = {};
    event.type = XR_TYPE_EVENT_DATA_BUFFER;
  }

  if (!m_sessionRunning)
    return;

  // Sync actions
  XrActiveActionSet activeActionSet{m_xrActionSet, XR_NULL_PATH};
  XrActionsSyncInfo syncInfo{};
  syncInfo.type = XR_TYPE_ACTIONS_SYNC_INFO;
  syncInfo.next = nullptr;
  syncInfo.countActiveActionSets = 1;
  syncInfo.activeActionSets = &activeActionSet;
  xrSyncActions(m_xrSession, &syncInfo);

  // Get predicted display time
  XrFrameState frameState{};
  frameState.type = XR_TYPE_FRAME_STATE;
  XrFrameWaitInfo waitInfo{};
  waitInfo.type = XR_TYPE_FRAME_WAIT_INFO;
  waitInfo.next = nullptr;
  if (XR_SUCCEEDED(xrWaitFrame(m_xrSession, &waitInfo, &frameState)))
  {
    m_predictedDisplayTime = frameState.predictedDisplayTime;

    // We need to call xrBeginFrame/xrEndFrame even if we don't render
    XrFrameBeginInfo beginInfo{};
    beginInfo.type = XR_TYPE_FRAME_BEGIN_INFO;
    beginInfo.next = nullptr;
    xrBeginFrame(m_xrSession, &beginInfo);

    if (m_settings.enableControllers)
    {
      pollControllers();
    }

    if (m_settings.enableHandTracking && m_handTrackingSupported)
    {
      pollHandTracking();
    }

    XrFrameEndInfo endInfo{};
    endInfo.type = XR_TYPE_FRAME_END_INFO;
    endInfo.next = nullptr;
    endInfo.displayTime = frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = 0;
    endInfo.layers = nullptr;
    xrEndFrame(m_xrSession, &endInfo);
  }
}

void OpenXRProtocol::pollControllers()
{
  for (int hand = 0; hand < 2; ++hand)
  {
    ControllerData data;

    // Get hand space location
    if (m_handSpaces[hand])
    {
      XrSpaceLocation spaceLocation{};
      spaceLocation.type = XR_TYPE_SPACE_LOCATION;
      XrResult result = xrLocateSpace(m_handSpaces[hand], m_xrSpace, m_predictedDisplayTime, &spaceLocation);

      if (XR_SUCCEEDED(result) && (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT))
      {
        data.active = true;
        data.position.x = spaceLocation.pose.position.x;
        data.position.y = spaceLocation.pose.position.y;
        data.position.z = spaceLocation.pose.position.z;

        if (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)
        {
          data.orientation.x = spaceLocation.pose.orientation.x;
          data.orientation.y = spaceLocation.pose.orientation.y;
          data.orientation.z = spaceLocation.pose.orientation.z;
          data.orientation.w = spaceLocation.pose.orientation.w;
        }
      }
    }

    // Get action states
    XrActionStateGetInfo getInfo{};
    getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
    getInfo.next = nullptr;
    getInfo.subactionPath = m_handPaths[hand];

    // Grab (squeeze)
    if (m_grabAction)
    {
      XrActionStateFloat grabState{};
      grabState.type = XR_TYPE_ACTION_STATE_FLOAT;
      getInfo.action = m_grabAction;
      if (XR_SUCCEEDED(xrGetActionStateFloat(m_xrSession, &getInfo, &grabState)))
      {
        data.grab = grabState.currentState;
      }
    }

    // Trigger
    if (m_triggerAction)
    {
      XrActionStateFloat triggerState{};
      triggerState.type = XR_TYPE_ACTION_STATE_FLOAT;
      getInfo.action = m_triggerAction;
      if (XR_SUCCEEDED(xrGetActionStateFloat(m_xrSession, &getInfo, &triggerState)))
      {
        data.trigger = triggerState.currentState;
      }
    }

    updateControllerParams(hand, data);
    m_controllers[hand] = data;
  }
}

void OpenXRProtocol::pollHandTracking()
{
  auto locateHandJoints = (PFN_xrLocateHandJointsEXT)m_xrLocateHandJointsEXT;
  if (!locateHandJoints)
    return;

  for (int hand = 0; hand < 2; ++hand)
  {
    if (!m_handTrackers[hand])
      continue;

    HandJointData data;

    XrHandJointLocationEXT jointLocations[XR_HAND_JOINT_COUNT_EXT];
    XrHandJointLocationsEXT locations{};
    locations.type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT;
    locations.next = nullptr;
    locations.jointCount = XR_HAND_JOINT_COUNT_EXT;
    locations.jointLocations = jointLocations;

    XrHandJointsLocateInfoEXT locateInfo{};
    locateInfo.type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT;
    locateInfo.next = nullptr;
    locateInfo.baseSpace = m_xrSpace;
    locateInfo.time = m_predictedDisplayTime;

    XrResult result = locateHandJoints(m_handTrackers[hand], &locateInfo, &locations);
    if (XR_SUCCEEDED(result) && locations.isActive)
    {
      data.tracked = true;

      for (int j = 0; j < 26; ++j)
      {
        const auto& loc = jointLocations[j];
        data.valid[j] = (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;

        if (data.valid[j])
        {
          data.positions[j].x = loc.pose.position.x;
          data.positions[j].y = loc.pose.position.y;
          data.positions[j].z = loc.pose.position.z;

          if (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)
          {
            data.orientations[j].x = loc.pose.orientation.x;
            data.orientations[j].y = loc.pose.orientation.y;
            data.orientations[j].z = loc.pose.orientation.z;
            data.orientations[j].w = loc.pose.orientation.w;
          }

          data.radii[j] = loc.radius;
        }
      }
    }

    updateHandParams(hand, data);
    m_hands[hand] = data;
  }
}

void OpenXRProtocol::updateControllerParams(int hand, const ControllerData& data)
{
  if (!m_device)
    return;

  auto& root = m_device->get_root_node();
  auto* controllers = root.find_child(std::string_view("controllers"));
  if (!controllers)
    return;

  const char* name = (hand == 0) ? "left" : "right";
  auto* handNode = controllers->find_child(std::string_view(name));
  if (!handNode)
    return;

  using TB = Tracking::TreeBuilder;
  TB::update_param(handNode, "active", data.active);
  TB::update_param(handNode, "position",
                   ossia::vec3f{data.position.x, data.position.y, data.position.z});
  TB::update_param(handNode, "orientation",
                   ossia::vec4f{data.orientation.x, data.orientation.y,
                                data.orientation.z, data.orientation.w});
  TB::update_param(handNode, "grab", data.grab);
  TB::update_param(handNode, "trigger", data.trigger);
}

void OpenXRProtocol::updateHandParams(int hand, const HandJointData& data)
{
  if (!m_device)
    return;

  auto& root = m_device->get_root_node();
  auto* hands = root.find_child(std::string_view("hands"));
  if (!hands)
    return;

  const char* name = (hand == 0) ? "left" : "right";
  auto* handNode = hands->find_child(std::string_view(name));
  if (!handNode)
    return;

  using TB = Tracking::TreeBuilder;
  TB::update_param(handNode, "tracked", data.tracked);

  auto* jointsNode = handNode->find_child(std::string_view("joints"));
  if (!jointsNode)
    return;

  for (int j = 0; j < 26; ++j)
  {
    auto* jointNode = jointsNode->find_child(std::string_view(jointNames[j]));
    if (!jointNode)
      continue;

    TB::update_param(jointNode, "valid", data.valid[j]);
    if (data.valid[j])
    {
      TB::update_param(jointNode, "position",
                       ossia::vec3f{data.positions[j].x, data.positions[j].y, data.positions[j].z});
      TB::update_param(jointNode, "orientation",
                       ossia::vec4f{data.orientations[j].x, data.orientations[j].y,
                                    data.orientations[j].z, data.orientations[j].w});
      TB::update_param(jointNode, "radius", data.radii[j]);
    }
  }
}

}
