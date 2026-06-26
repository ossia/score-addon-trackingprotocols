#include "PSNProtocol.hpp"
#include "../Common/TrackingTreeBuilder.hpp"

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>

#include <psn_decoder_impl.hpp>

namespace PSN
{

PSNProtocol::PSNProtocol(
    const ossia::net::network_context_ptr& ctx,
    const PSNSpecificSettings& settings)
    : ossia::net::protocol_base{}
    , m_ctx{ctx}
    , m_settings{settings}
    , m_tracker_slots{settings.numTrackers, Tracking::SlotStrategy::PersistentID}
{
}

PSNProtocol::~PSNProtocol()
{
  stop_receive();
}

void PSNProtocol::set_device(ossia::net::device_base& dev)
{
  m_device = &dev;

  auto& root = dev.get_root_node();
  create_device_tree(root);

  setup_receive_socket();
}

void PSNProtocol::create_device_tree(ossia::net::node_base& root)
{
  using TB = Tracking::TreeBuilder;

  // System-level parameters
  TB::create_string_param(root, "system_name", "");
  TB::create_int_param(root, "frame");
  TB::create_int_param(root, "timestamp");

  // Create tracker slots
  auto* trackers_node = root.create_child("trackers");
  for (int i = 0; i < m_settings.numTrackers; ++i)
  {
    auto* slot_node = trackers_node->create_child(std::to_string(i));

    TB::create_bool_param(*slot_node, "active", false);
    TB::create_int_param(*slot_node, "id", -1);
    TB::create_string_param(*slot_node, "name", "");
    TB::create_vec3f_param(*slot_node, "position");

    if (m_settings.enableOrientation)
    {
      TB::create_vec3f_param(*slot_node, "orientation", {0.f, 0.f, 0.f},
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

    TB::create_float_param(*slot_node, "status", 0.f, 0.f, 1.f);

    if (m_settings.enableTargetPosition)
    {
      TB::create_vec3f_param(*slot_node, "target_position");
    }

    TB::create_int_param(*slot_node, "timestamp");
  }
}

void PSNProtocol::setup_receive_socket()
{
  try
  {
    ossia::net::inbound_socket_configuration config{
        .bind = "0.0.0.0",
        .port = m_settings.port,
        .multicast_group = m_settings.multicastAddress.toStdString(),
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
    ossia::logger().error("PSN: Failed to setup receive socket: {}", e.what());
  }
}

void PSNProtocol::stop_receive()
{
  if (m_receive_socket)
  {
    m_receive_socket->close();
    m_receive_socket.reset();
  }
}

void PSNProtocol::on_received_data(const char* data, std::size_t size)
{
  if (!m_device)
    return;

  if (m_decoder.decode(data, size))
  {
    const auto& info = m_decoder.get_info();
    const auto& pkt_data = m_decoder.get_data();

    // Drop retransmits: the header's frame_id increments per transmitted
    // packet, so two consecutive equal values signal a duplicate.
    const uint8_t incoming_frame_id
        = !pkt_data.trackers.empty() ? pkt_data.header.frame_id : info.header.frame_id;
    if (m_have_prev_frame_id && incoming_frame_id == m_prev_frame_id)
      return;
    m_prev_frame_id = incoming_frame_id;
    m_have_prev_frame_id = true;

    if (!info.system_name.empty() || !info.tracker_names.empty())
    {
      process_info_packet(info);
    }

    if (!pkt_data.trackers.empty())
    {
      process_data_packet(pkt_data);
    }
  }
}

void PSNProtocol::process_info_packet(const ::psn::psn_decoder::info_t& info)
{
  auto& root = m_device->get_root_node();

  // Update system name
  if (!info.system_name.empty() && info.system_name != m_system_name)
  {
    m_system_name = info.system_name;
    Tracking::TreeBuilder::update_param(&root, "system_name", m_system_name);
  }

  // Update frame info
  m_frame_id = info.header.frame_id;
  m_timestamp = info.header.timestamp_usec;
  Tracking::TreeBuilder::update_param(&root, "frame", (int)m_frame_id);
  Tracking::TreeBuilder::update_param(&root, "timestamp", (int)(m_timestamp / 1000));

  // Update tracker names
  auto* trackers_node = root.find_child(std::string_view("trackers"));
  if (!trackers_node)
    return;

  for (const auto& [tracker_id, tracker_name] : info.tracker_names)
  {
    int slot = m_tracker_slots.find_or_allocate(tracker_id);
    if (slot >= 0 && slot < m_settings.numTrackers)
    {
      auto& slot_info = m_tracker_slots.slot_info(slot);
      slot_info.name = tracker_name;

      if (auto* slot_node = trackers_node->find_child(std::to_string(slot)))
      {
        Tracking::TreeBuilder::update_param(slot_node, "id", tracker_id);
        Tracking::TreeBuilder::update_param(slot_node, "name", tracker_name);
      }
    }
  }
}

void PSNProtocol::process_data_packet(const ::psn::psn_decoder::data_t& data)
{
  auto& root = m_device->get_root_node();

  // Update frame info
  m_frame_id = data.header.frame_id;
  m_timestamp = data.header.timestamp_usec;
  Tracking::TreeBuilder::update_param(&root, "frame", (int)m_frame_id);
  Tracking::TreeBuilder::update_param(&root, "timestamp", (int)(m_timestamp / 1000));

  // Process trackers
  for (const auto& [tracker_id, tracker] : data.trackers)
  {
    int slot = m_tracker_slots.find_or_allocate(tracker_id);
    if (slot >= 0 && slot < m_settings.numTrackers)
    {
      m_tracker_slots.slot_info(slot).active = true;
      update_tracker_parameters(slot, tracker);
    }
  }
}

void PSNProtocol::update_tracker_parameters(int slot, const ::psn::tracker& tracker)
{
  auto& root = m_device->get_root_node();

  auto* trackers_node = root.find_child(std::string_view("trackers"));
  if (!trackers_node)
    return;

  auto* slot_node = trackers_node->find_child(std::to_string(slot));
  if (!slot_node)
    return;

  using TB = Tracking::TreeBuilder;

  TB::update_param(slot_node, "active", true);
  TB::update_param(slot_node, "id", (int)tracker.get_id());

  if (!tracker.get_name().empty())
  {
    TB::update_param(slot_node, "name", tracker.get_name());
  }

  if (tracker.is_pos_set())
  {
    auto pos = tracker.get_pos();
    TB::update_param(slot_node, "position", ossia::vec3f{pos.x, pos.y, pos.z});
  }

  if (m_settings.enableOrientation && tracker.is_ori_set())
  {
    auto ori = tracker.get_ori();
    TB::update_param(slot_node, "orientation", ossia::vec3f{ori.x, ori.y, ori.z});
  }

  if (m_settings.enableVelocity && tracker.is_speed_set())
  {
    auto vel = tracker.get_speed();
    TB::update_param(slot_node, "velocity", ossia::vec3f{vel.x, vel.y, vel.z});
  }

  if (m_settings.enableAcceleration && tracker.is_accel_set())
  {
    auto acc = tracker.get_accel();
    TB::update_param(slot_node, "acceleration", ossia::vec3f{acc.x, acc.y, acc.z});
  }

  if (tracker.is_status_set())
  {
    TB::update_param(slot_node, "status", tracker.get_status());
  }

  if (m_settings.enableTargetPosition && tracker.is_target_pos_set())
  {
    auto tgt = tracker.get_target_pos();
    TB::update_param(slot_node, "target_position", ossia::vec3f{tgt.x, tgt.y, tgt.z});
  }

  if (tracker.is_timestamp_set())
  {
    TB::update_param(slot_node, "timestamp", (int)(tracker.get_timestamp() / 1000));
  }
}

}
