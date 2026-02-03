#pragma once

#include <cstdint>
#include <chrono>
#include <optional>
#include <string>

namespace Tracking
{

// 2D/3D Position
struct Position2D
{
  float x{}, y{};
};

struct Position3D
{
  float x{}, y{}, z{};
};

// Orientation (Euler angles in radians)
struct Orientation2D
{
  float angle{};
};

struct Orientation3D
{
  float roll{}, pitch{}, yaw{};
};

// Quaternion orientation
struct Quaternion
{
  float x{}, y{}, z{}, w{1.0f};
};

// Velocity
struct Velocity2D
{
  float x{}, y{};
  float angular{};
};

struct Velocity3D
{
  float x{}, y{}, z{};
  float angular{};
};

// Acceleration
struct Acceleration2D
{
  float motion{};
  float rotation{};
};

struct Acceleration3D
{
  float x{}, y{}, z{};
  float motion{};
  float rotation{};
};

// Generic tracked entity base
struct TrackedEntityBase
{
  int32_t id{-1};
  std::string name;
  bool active{false};
  std::chrono::steady_clock::time_point last_update;
};

// TrackedPoint - for cursors, markers, LEDs
struct TrackedPoint : TrackedEntityBase
{
  Position3D position;
  Velocity3D velocity;
  Acceleration3D acceleration;

  // Optional index for LED-like markers
  std::optional<int> index;

  // Optional latency
  std::optional<uint16_t> latency;
};

// TrackedObject - for rigid bodies with orientation
struct TrackedObject : TrackedEntityBase
{
  Position3D position;
  Orientation3D euler;
  Quaternion quaternion;
  Velocity3D velocity;
  Acceleration3D acceleration;

  // Optional class/type identifier
  std::optional<int32_t> class_id;
  std::optional<int32_t> type_id;
  std::optional<int32_t> user_id;
  std::optional<int32_t> component_id;

  // Optional latency
  std::optional<uint16_t> latency;

  // Optional target position (PSN)
  std::optional<Position3D> target_position;

  // Optional status (PSN)
  std::optional<float> status;
};

// TrackedBounds - for blobs, bounds
struct TrackedBounds : TrackedEntityBase
{
  Position3D position;
  Orientation3D euler;
  Velocity3D velocity;
  Acceleration3D acceleration;

  // Size and area
  float width{}, height{}, depth{};
  float area{}, volume{};
};

// TrackedZone - for RTTrP zones
struct TrackedZone
{
  std::string name;
  bool occupied{false};
};

// LED marker for RTTrP
struct LEDMarker
{
  int index{-1};
  bool active{false};
  Position3D position;
  Velocity3D velocity;
  Acceleration3D acceleration;
  std::optional<uint16_t> latency;
};

// Slot management strategy
enum class SlotStrategy
{
  SessionBased,  // TUIO: session IDs are transient
  PersistentID,  // PSN: tracker IDs are stable
  NameBased      // RTTrP: trackables identified by name
};

}
