#pragma once

#include <cstdint>
#include <chrono>
#include <vector>
#include <optional>

namespace TUIO
{

// TUIO 2.0 data structures
struct TUIO2Token
{
  uint32_t session_id{0};
  uint16_t user_id{0};
  uint16_t type_id{0};
  uint32_t component_id{0};
  float x{}, y{}, angle{};
  // Optional velocity/acceleration
  std::optional<float> x_vel, y_vel, a_vel, m_acc, r_acc;
  std::chrono::steady_clock::time_point last_update;
};

struct TUIO2Pointer
{
  uint32_t session_id{0};
  uint16_t user_id{0};
  uint16_t type_id{0};
  uint32_t component_id{0};
  float x{}, y{}, angle{}, shear{}, radius{}, pressure{};
  // Optional velocity/acceleration
  std::optional<float> x_vel, y_vel, p_vel, m_acc, p_acc;
  std::chrono::steady_clock::time_point last_update;
};

struct TUIO2Bounds
{
  uint32_t session_id{0};
  float x{}, y{}, angle{}, width{}, height{}, area{};
  // Optional velocity/acceleration
  std::optional<float> x_vel, y_vel, a_vel, m_acc, r_acc;
  std::chrono::steady_clock::time_point last_update;
};

struct TUIO2Symbol
{
  uint32_t session_id{0};
  uint16_t user_id{0};
  uint16_t type_id{0};
  uint32_t component_id{0};
  std::string group;
  std::string data;
};

// 3D variants
struct TUIO2Token3D
{
  uint32_t session_id{0};
  uint16_t user_id{0};
  uint16_t type_id{0};
  uint32_t component_id{0};
  float x{}, y{}, z{};
  float angle{}, x_axis{}, y_axis{}, z_axis{};
  // Optional velocity/acceleration
  std::optional<float> x_vel, y_vel, z_vel, r_vel, m_acc, r_acc;
  std::chrono::steady_clock::time_point last_update;
};

struct TUIO2Pointer3D
{
  uint32_t session_id{0};
  uint16_t user_id{0};
  uint16_t type_id{0};
  uint32_t component_id{0};
  float x{}, y{}, z{};
  float x_axis{}, y_axis{}, z_axis{}, radius{};
  // Optional velocity/acceleration
  std::optional<float> x_vel, y_vel, z_vel, r_vel, m_acc, r_acc;
  std::chrono::steady_clock::time_point last_update;
};

struct TUIO2Bounds3D
{
  uint32_t session_id{0};
  float x{}, y{}, z{};
  float angle{}, x_axis{}, y_axis{}, z_axis{};
  float width{}, height{}, depth{}, volume{};
  // Optional velocity/acceleration
  std::optional<float> x_vel, y_vel, z_vel, r_vel, m_acc, r_acc;
  std::chrono::steady_clock::time_point last_update;
};

// Control message data
struct TUIO2Control
{
  uint32_t session_id{0};
  std::vector<float> values; // normalized -1.0 to 1.0 or bool as 0/1
};

// Default Pointer Type IDs for TUIO 2.0
enum class TUIO2PointerType : uint16_t
{
  Unknown = 0,
  // Right hand fingers
  RightIndex = 1,
  RightMiddle = 2,
  RightRing = 3,
  RightLittle = 4,
  RightThumb = 5,
  // Left hand fingers
  LeftIndex = 6,
  LeftMiddle = 7,
  LeftRing = 8,
  LeftLittle = 9,
  LeftThumb = 10,
  // Pointer devices
  Stylus = 11,
  LaserPointer = 12,
  Mouse = 13,
  Trackball = 14,
  Joystick = 15,
  Remote = 16,
  // Body parts
  RightHandPointing = 21,
  RightHandOpen = 22,
  RightHandClosed = 23,
  LeftHandPointing = 24,
  LeftHandOpen = 25,
  LeftHandClosed = 26,
  RightFoot = 27,
  LeftFoot = 28,
  Head = 29,
  Person = 30,
  // Custom types start at 64
  CustomStart = 64
};

}