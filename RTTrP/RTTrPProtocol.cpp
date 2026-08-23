#include "RTTrPProtocol.hpp"
#include "../Common/TrackingTreeBuilder.hpp"

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/name_validation.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>


namespace RTTrP
{

RTTrPProtocol::RTTrPProtocol(
    const ossia::net::network_context_ptr& ctx,
    const RTTrPSpecificSettings& settings)
    : ossia::net::protocol_base{}
    , m_ctx{ctx}
    , m_settings{settings}
    , m_trackable_slots{settings.numTrackables, Tracking::SlotStrategy::NameBased}
{
}

RTTrPProtocol::~RTTrPProtocol()
{
  stop_receive();
}

void RTTrPProtocol::set_device(ossia::net::device_base& dev)
{
  m_device = &dev;

  auto& root = dev.get_root_node();
  create_device_tree(root);

  setup_receive_socket();
}

void RTTrPProtocol::create_device_tree(ossia::net::node_base& root)
{
  using TB = Tracking::TreeBuilder;

  // Frame-level parameters
  TB::create_int_param(root, "frame");
  TB::create_int_param(root, "timestamp");

  // Create trackable slots
  auto* trackables_node = root.create_child("trackables");
  for (int i = 0; i < m_settings.numTrackables; ++i)
  {
    auto* slot_node = trackables_node->create_child(std::to_string(i));

    TB::create_bool_param(*slot_node, "active", false);
    TB::create_string_param(*slot_node, "name", "");
    TB::create_vec3f_param(*slot_node, "position");

    if (m_settings.enableQuaternion)
    {
      TB::create_vec4f_param(*slot_node, "quaternion", {0.f, 0.f, 0.f, 1.f});
    }

    if (m_settings.enableEuler)
    {
      TB::create_vec3f_param(*slot_node, "euler", {0.f, 0.f, 0.f},
                             static_cast<float>(-M_PI), static_cast<float>(M_PI));
    }

    if (m_settings.enableVelocity)
    {
      TB::create_vec3f_param(*slot_node, "velocity");
    }

    if (m_settings.enableAcceleration)
    {
      TB::create_vec3f_param(*slot_node, "acceleration");
    }

    TB::create_int_param(*slot_node, "latency");

    // Create LED slots
    if (m_settings.maxLEDsPerTrackable > 0)
    {
      TB::create_led_slots(*slot_node, m_settings.maxLEDsPerTrackable);
    }
  }

  // Create zones container (populated dynamically)
  if (m_settings.enableZones)
  {
    root.create_child("zones");
  }
}

void RTTrPProtocol::setup_receive_socket()
{
  try
  {
    ossia::net::inbound_socket_configuration config{
        .bind = "0.0.0.0",
        .port = m_settings.port
    };

    m_receive_socket = std::make_unique<ossia::net::udp_receive_socket>(
        config, m_ctx->context);

    m_receive_socket->open();
    m_receive_socket->receive(
        [this](const char* data, std::size_t sz)
        {
          this->on_received_data(data, sz);
        });
  }
  catch (const std::exception& e)
  {
    ossia::logger().error("RTTrP: Failed to setup receive socket: {}", e.what());
  }
}

void RTTrPProtocol::stop_receive()
{
  if (m_receive_socket)
  {
    m_receive_socket->close();
    m_receive_socket.reset();
  }
}

void RTTrPProtocol::on_received_data(const char* data, std::size_t size)
{
  if (!m_device)
    return;

  process_packet(data, size);
}

bool RTTrPProtocol::process_packet(const char* data, std::size_t size)
{
  if (!m_device)
    return false;

  if (!parse_packet(reinterpret_cast<const uint8_t*>(data), size, m_parsed))
    return false;

  // Drop duplicate packets (retransmit / loopback echo).
  const uint32_t packet_id = m_parsed.header.packet_id;
  if (m_have_prev_packet_id && packet_id == m_prev_packet_id)
    return false;
  m_prev_packet_id = packet_id;
  m_have_prev_packet_id = true;

  auto& root = m_device->get_root_node();
  Tracking::TreeBuilder::update_param(&root, "frame", (int)packet_id);

  for (const auto& trackable : m_parsed.trackables)
  {
    int slot = m_trackable_slots.find_or_allocate(trackable.name);
    if (slot < 0 || slot >= m_settings.numTrackables)
      continue;

    m_trackable_slots.slot_info(slot).active = true;
    update_trackable_parameters(slot, trackable);

    if (trackable.has_timestamp)
      Tracking::TreeBuilder::update_param(&root, "timestamp", (int)trackable.timestamp);

    for (const auto& zone : trackable.zones)
      update_zone_parameter(zone, true);
  }

  return true;
}

void RTTrPProtocol::update_trackable_parameters(int slot, const ParsedTrackable& trackable)
{
  auto& root = m_device->get_root_node();

  auto* trackables_node = root.find_child(std::string_view("trackables"));
  if (!trackables_node)
    return;

  auto* slot_node = trackables_node->find_child(std::to_string(slot));
  if (!slot_node)
    return;

  using TB = Tracking::TreeBuilder;

  TB::update_param(slot_node, "active", true);
  TB::update_param(slot_node, "name", trackable.name);

  if (trackable.has_centroid)
  {
    TB::update_param(slot_node, "position",
                     ossia::vec3f{static_cast<float>(trackable.x),
                                  static_cast<float>(trackable.y),
                                  static_cast<float>(trackable.z)});
    TB::update_param(slot_node, "latency", (int)trackable.latency);
  }

  if (m_settings.enableQuaternion && trackable.has_quaternion)
  {
    TB::update_param(slot_node, "quaternion",
                     ossia::vec4f{static_cast<float>(trackable.qx),
                                  static_cast<float>(trackable.qy),
                                  static_cast<float>(trackable.qz),
                                  static_cast<float>(trackable.qw)});
  }

  if (m_settings.enableEuler && trackable.has_euler)
  {
    TB::update_param(slot_node, "euler",
                     ossia::vec3f{static_cast<float>(trackable.roll),
                                  static_cast<float>(trackable.pitch),
                                  static_cast<float>(trackable.yaw)});
  }

  if (m_settings.enableVelocity && trackable.has_velocity)
  {
    TB::update_param(slot_node, "velocity",
                     ossia::vec3f{trackable.vx, trackable.vy, trackable.vz});
  }

  if (m_settings.enableAcceleration && trackable.has_velocity)
  {
    TB::update_param(slot_node, "acceleration",
                     ossia::vec3f{trackable.ax, trackable.ay, trackable.az});
  }

  // Update LED markers
  if (!trackable.leds.empty())
  {
    auto* leds_node = slot_node->find_child(std::string_view("leds"));
    if (leds_node)
    {
      for (const auto& led : trackable.leds)
      {
        if (led.index >= m_settings.maxLEDsPerTrackable)
          continue;

        if (auto* led_node = leds_node->find_child(std::to_string(led.index)))
        {
          TB::update_param(led_node, "active", true);
          TB::update_param(led_node, "position",
                           ossia::vec3f{static_cast<float>(led.x),
                                        static_cast<float>(led.y),
                                        static_cast<float>(led.z)});

          if (led.has_velocity)
          {
            TB::update_param(led_node, "velocity",
                             ossia::vec3f{led.vx, led.vy, led.vz});
            TB::update_param(led_node, "acceleration",
                             ossia::vec3f{led.ax, led.ay, led.az});
          }
        }
      }
    }
  }
}

void RTTrPProtocol::update_zone_parameter(const std::string& zone_name, bool occupied)
{
  if (!m_settings.enableZones || zone_name.empty())
    return;

  auto& root = m_device->get_root_node();
  auto* zones_node = root.find_child(std::string_view("zones"));
  if (!zones_node)
    return;

  // Zone names come off the wire unvalidated; sanitize before using them as
  // ossia node names so spaces/slashes/non-ASCII don't produce invalid paths.
  const std::string safe_name = ossia::net::sanitize_name(zone_name);
  if (safe_name.empty())
    return;

  auto it = m_zone_nodes.find(safe_name);
  ossia::net::node_base* zone_node = nullptr;

  if (it == m_zone_nodes.end())
  {
    zone_node = Tracking::TreeBuilder::create_zone_slot(*zones_node, safe_name);
    m_zone_nodes[safe_name] = zone_node;
  }
  else
  {
    zone_node = it->second;
  }

  Tracking::TreeBuilder::update_param(zone_node, "occupied", occupied);
}

}
