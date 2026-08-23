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
  process_packet(data, size);
}

bool PSNProtocol::process_packet(const char* data, std::size_t size)
{
  if (!m_device)
    return false;

  if (!m_decoder.decode(data, size))
    return false;

  // The decoder hands out the last *complete* info and data frames; apply
  // each of them once, when its frame id changes. A frame split over several
  // packets thus lands whole, a retransmitted frame is applied once, and info
  // and data frames - independent counters - never shadow each other.
  bool applied = false;

  // A decoded header has a packet count of at least one; the decoder also
  // fills info.tracker_names with empty names for the trackers it sees in
  // data packets, so that map is no sign of an info frame.
  const auto& info = m_decoder.get_info();
  if (info.header.frame_packet_count > 0 && m_last_info_frame != info.header.frame_id)
  {
    m_last_info_frame = info.header.frame_id;
    process_info_packet(info);
    applied = true;
  }

  const auto& pkt_data = m_decoder.get_data();
  if (pkt_data.header.frame_packet_count > 0 && !pkt_data.trackers.empty()
      && m_last_data_frame != pkt_data.header.frame_id)
  {
    m_last_data_frame = pkt_data.header.frame_id;
    process_data_packet(pkt_data);
    applied = true;
  }

  return applied;
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
