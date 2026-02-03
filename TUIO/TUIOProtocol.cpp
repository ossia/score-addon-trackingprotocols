#include "TUIOProtocol.hpp"
#include "../Common/TrackingTreeBuilder.hpp"

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/domain/domain.hpp>

#include <cmath>

namespace TUIO
{

TUIOProtocol::TUIOProtocol(
    const ossia::net::network_context_ptr& ctx,
    uint16_t port,
    int numObjects,
    int numCursors,
    int numBlobs,
    TUIOVersion version)
    : ossia::net::protocol_base{}
    , m_ctx{ctx}
    , m_object_slots{numObjects, Tracking::SlotStrategy::SessionBased}
    , m_cursor_slots{numCursors, Tracking::SlotStrategy::SessionBased}
    , m_blob_slots{numBlobs, Tracking::SlotStrategy::SessionBased}
    , m_port{port}
    , m_num_objects{numObjects}
    , m_num_cursors{numCursors}
    , m_num_blobs{numBlobs}
    , m_version{version}
{
  if (m_version == TUIOVersion::V2_0)
  {
    m_tuio2_tokens.resize(numObjects);
    m_tuio2_pointers.resize(numCursors);
    m_tuio2_bounds.resize(numBlobs);
    m_tuio2_symbols.resize(numObjects);
    m_tuio2_controls.resize(numObjects);
  }
}

TUIOProtocol::~TUIOProtocol()
{
  stop_receive();
}

void TUIOProtocol::set_device(ossia::net::device_base& dev)
{
  m_device = &dev;

  auto& root = dev.get_root_node();

  // Create common top-level nodes using TreeBuilder
  using TB = Tracking::TreeBuilder;
  TB::create_string_param(root, "source", "unknown");
  TB::create_int_param(root, "frame", 0);

  if (m_version == TUIOVersion::V1_1)
  {
    create_tuio1_tree(root);
  }
  else
  {
    create_tuio2_tree(root);
  }

  setup_receive_socket();
}

void TUIOProtocol::create_tuio1_tree(ossia::net::node_base& root)
{
  using TB = Tracking::TreeBuilder;

  // Create 2Dobj profile nodes
  auto* obj_node = root.create_child("2Dobj");
  for (int i = 0; i < m_num_objects; ++i)
  {
    auto* slot_node = obj_node->create_child(std::to_string(i));

    TB::create_int_param(*slot_node, "session_id", -1);
    TB::create_bool_param(*slot_node, "active", false);
    TB::create_int_param(*slot_node, "class_id", 0);
    TB::create_vec2f_param(*slot_node, "position", {0.5f, 0.5f});
    TB::create_float_param(*slot_node, "angle", 0.f, 0.f, static_cast<float>(2 * M_PI));
    TB::create_vec2f_param(*slot_node, "velocity", {0.f, 0.f}, -10.f, 10.f);
    TB::create_float_param(*slot_node, "angle_velocity", 0.f, static_cast<float>(-2 * M_PI), static_cast<float>(2 * M_PI));
    TB::create_float_param(*slot_node, "motion_acceleration", 0.f, 0.f, 100.f);
    TB::create_float_param(*slot_node, "rotation_acceleration", 0.f, 0.f, 100.f);
  }

  // Create 2Dcur profile nodes
  auto* cur_node = root.create_child("2Dcur");
  for (int i = 0; i < m_num_cursors; ++i)
  {
    auto* slot_node = cur_node->create_child(std::to_string(i));

    TB::create_int_param(*slot_node, "session_id", -1);
    TB::create_bool_param(*slot_node, "active", false);
    TB::create_vec2f_param(*slot_node, "position", {0.5f, 0.5f});
    TB::create_vec2f_param(*slot_node, "velocity", {0.f, 0.f}, -10.f, 10.f);
    TB::create_float_param(*slot_node, "motion_acceleration", 0.f, 0.f, 100.f);
  }

  // Create 2Dblb profile nodes
  auto* blob_node = root.create_child("2Dblb");
  for (int i = 0; i < m_num_blobs; ++i)
  {
    auto* slot_node = blob_node->create_child(std::to_string(i));

    TB::create_int_param(*slot_node, "session_id", -1);
    TB::create_bool_param(*slot_node, "active", false);
    TB::create_vec2f_param(*slot_node, "position", {0.5f, 0.5f});
    TB::create_float_param(*slot_node, "angle", 0.f, 0.f, static_cast<float>(2 * M_PI));
    TB::create_vec2f_param(*slot_node, "size", {0.1f, 0.1f});
    TB::create_float_param(*slot_node, "area", 0.01f, 0.f, 1.f);
    TB::create_vec2f_param(*slot_node, "velocity", {0.f, 0.f}, -10.f, 10.f);
    TB::create_float_param(*slot_node, "angle_velocity", 0.f, static_cast<float>(-2 * M_PI), static_cast<float>(2 * M_PI));
    TB::create_float_param(*slot_node, "motion_acceleration", 0.f, 0.f, 100.f);
    TB::create_float_param(*slot_node, "rotation_acceleration", 0.f, 0.f, 100.f);
  }
}

void TUIOProtocol::create_tuio2_tree(ossia::net::node_base& root)
{
  using TB = Tracking::TreeBuilder;

  // Create tok (token) profile nodes
  auto* tok_node = root.create_child("tok");
  for (int i = 0; i < m_num_objects; ++i)
  {
    auto* slot_node = tok_node->create_child(std::to_string(i));

    TB::create_int_param(*slot_node, "session_id", -1);
    TB::create_bool_param(*slot_node, "active", false);
    TB::create_int_param(*slot_node, "user_id", 0);
    TB::create_int_param(*slot_node, "type_id", 0);
    TB::create_int_param(*slot_node, "component_id", 0);
    TB::create_vec2f_param(*slot_node, "position", {0.5f, 0.5f});
    TB::create_float_param(*slot_node, "angle", 0.f, 0.f, static_cast<float>(2 * M_PI));
    TB::create_vec2f_param(*slot_node, "velocity", {0.f, 0.f}, -10.f, 10.f);
    TB::create_float_param(*slot_node, "angle_velocity", 0.f, static_cast<float>(-2 * M_PI), static_cast<float>(2 * M_PI));
    TB::create_float_param(*slot_node, "motion_acceleration", 0.f, 0.f, 100.f);
    TB::create_float_param(*slot_node, "rotation_acceleration", 0.f, 0.f, 100.f);
    TB::create_string_param(*slot_node, "symbol_group", "");
    TB::create_string_param(*slot_node, "symbol_data", "");
  }

  // Create ptr (pointer) profile nodes
  auto* ptr_node = root.create_child("ptr");
  for (int i = 0; i < m_num_cursors; ++i)
  {
    auto* slot_node = ptr_node->create_child(std::to_string(i));

    TB::create_int_param(*slot_node, "session_id", -1);
    TB::create_bool_param(*slot_node, "active", false);
    TB::create_int_param(*slot_node, "user_id", 0);
    TB::create_int_param(*slot_node, "type_id", 1);
    TB::create_int_param(*slot_node, "component_id", 0);
    TB::create_vec2f_param(*slot_node, "position", {0.5f, 0.5f});
    TB::create_float_param(*slot_node, "angle", 0.f, 0.f, static_cast<float>(2 * M_PI));
    TB::create_float_param(*slot_node, "shear", 0.f, 0.f, static_cast<float>(M_PI / 2));
    TB::create_float_param(*slot_node, "radius", 0.f, 0.f, 1.f);
    TB::create_float_param(*slot_node, "pressure", 1.f, -1.f, 1.f);
    TB::create_vec2f_param(*slot_node, "velocity", {0.f, 0.f}, -10.f, 10.f);
    TB::create_float_param(*slot_node, "pressure_velocity", 0.f, -10.f, 10.f);
    TB::create_float_param(*slot_node, "motion_acceleration", 0.f, 0.f, 100.f);
    TB::create_float_param(*slot_node, "pressure_acceleration", 0.f, 0.f, 100.f);
  }

  // Create bnd (bounds) profile nodes
  auto* bnd_node = root.create_child("bnd");
  for (int i = 0; i < m_num_blobs; ++i)
  {
    auto* slot_node = bnd_node->create_child(std::to_string(i));

    TB::create_int_param(*slot_node, "session_id", -1);
    TB::create_bool_param(*slot_node, "active", false);
    TB::create_vec2f_param(*slot_node, "position", {0.5f, 0.5f});
    TB::create_float_param(*slot_node, "angle", 0.f, 0.f, static_cast<float>(2 * M_PI));
    TB::create_vec2f_param(*slot_node, "size", {0.1f, 0.1f});
    TB::create_float_param(*slot_node, "area", 0.01f, 0.f, 1.f);
    TB::create_vec2f_param(*slot_node, "velocity", {0.f, 0.f}, -10.f, 10.f);
    TB::create_float_param(*slot_node, "angle_velocity", 0.f, static_cast<float>(-2 * M_PI), static_cast<float>(2 * M_PI));
    TB::create_float_param(*slot_node, "motion_acceleration", 0.f, 0.f, 100.f);
    TB::create_float_param(*slot_node, "rotation_acceleration", 0.f, 0.f, 100.f);
  }
}

void TUIOProtocol::setup_receive_socket()
{
  try
  {
    m_receive_socket = std::make_unique<ossia::net::udp_receive_socket>(
        ossia::net::inbound_socket_configuration{.port = m_port},
        m_ctx->context);

    m_receive_socket->open();
    m_receive_socket->receive(
        [this](const char* data, std::size_t sz)
        {
          if (!m_device)
            return;

          auto on_message = [this](auto&& msg) {
            this->on_received_message(msg);
          };

          using processor = ossia::net::osc_packet_processor<decltype(on_message)>;
          processor{on_message}(data, sz);
        });
  }
  catch (const std::exception& e)
  {
    ossia::logger().error("TUIO: Failed to setup receive socket: {}", e.what());
  }
}

void TUIOProtocol::stop_receive()
{
  if (m_receive_socket)
  {
    m_receive_socket->close();
    m_receive_socket.reset();
  }
}

void TUIOProtocol::on_received_message(const oscpack::ReceivedMessage& msg)
{
  std::string addr = msg.AddressPattern();

  if (m_version == TUIOVersion::V1_1)
  {
    if (addr == "/tuio/2Dobj")
      handle_2Dobj_message(msg);
    else if (addr == "/tuio/2Dcur")
      handle_2Dcur_message(msg);
    else if (addr == "/tuio/2Dblb")
      handle_2Dblb_message(msg);
  }
  else
  {
    if (addr == "/tuio2/frm")
      handle_tuio2_frm(msg);
    else if (addr == "/tuio2/tok")
      handle_tuio2_tok(msg);
    else if (addr == "/tuio2/ptr")
      handle_tuio2_ptr(msg);
    else if (addr == "/tuio2/bnd")
      handle_tuio2_bnd(msg);
    else if (addr == "/tuio2/sym")
      handle_tuio2_sym(msg);
    else if (addr == "/tuio2/ctl")
      handle_tuio2_ctl(msg);
    else if (addr == "/tuio2/alv")
      handle_tuio2_alv(msg);
  }
}

void TUIOProtocol::handle_2Dobj_message(const oscpack::ReceivedMessage& msg)
{
  try
  {
    auto it = msg.ArgumentsBegin();
    if (it == msg.ArgumentsEnd() || !it->IsString())
      return;

    std::string cmd = it->AsString();

    if (cmd == "source")
    {
      ++it;
      if (it != msg.ArgumentsEnd() && it->IsString())
      {
        m_source = it->AsString();
        Tracking::TreeBuilder::update_param(&m_device->get_root_node(), "source", m_source);
      }
    }
    else if (cmd == "alive")
    {
      process_2Dobj_alive(msg);
    }
    else if (cmd == "set")
    {
      process_2Dobj_set(msg);
    }
    else if (cmd == "fseq")
    {
      ++it;
      if (it != msg.ArgumentsEnd() && it->IsInt32())
      {
        m_frame_id = it->AsInt32();
        Tracking::TreeBuilder::update_param(&m_device->get_root_node(), "frame", m_frame_id);
      }
    }
  }
  catch (const oscpack::Exception& e)
  {
    ossia::logger().error("TUIO 2Dobj parse error: {}", e.what());
  }
}

void TUIOProtocol::handle_2Dcur_message(const oscpack::ReceivedMessage& msg)
{
  try
  {
    auto it = msg.ArgumentsBegin();
    if (it == msg.ArgumentsEnd() || !it->IsString())
      return;

    std::string cmd = it->AsString();

    if (cmd == "source")
    {
      ++it;
      if (it != msg.ArgumentsEnd() && it->IsString())
      {
        m_source = it->AsString();
        Tracking::TreeBuilder::update_param(&m_device->get_root_node(), "source", m_source);
      }
    }
    else if (cmd == "alive")
    {
      process_2Dcur_alive(msg);
    }
    else if (cmd == "set")
    {
      process_2Dcur_set(msg);
    }
    else if (cmd == "fseq")
    {
      ++it;
      if (it != msg.ArgumentsEnd() && it->IsInt32())
      {
        m_frame_id = it->AsInt32();
        Tracking::TreeBuilder::update_param(&m_device->get_root_node(), "frame", m_frame_id);
      }
    }
  }
  catch (const oscpack::Exception& e)
  {
    ossia::logger().error("TUIO 2Dcur parse error: {}", e.what());
  }
}

void TUIOProtocol::handle_2Dblb_message(const oscpack::ReceivedMessage& msg)
{
  try
  {
    auto it = msg.ArgumentsBegin();
    if (it == msg.ArgumentsEnd() || !it->IsString())
      return;

    std::string cmd = it->AsString();

    if (cmd == "source")
    {
      ++it;
      if (it != msg.ArgumentsEnd() && it->IsString())
      {
        m_source = it->AsString();
        Tracking::TreeBuilder::update_param(&m_device->get_root_node(), "source", m_source);
      }
    }
    else if (cmd == "alive")
    {
      process_2Dblb_alive(msg);
    }
    else if (cmd == "set")
    {
      process_2Dblb_set(msg);
    }
    else if (cmd == "fseq")
    {
      ++it;
      if (it != msg.ArgumentsEnd() && it->IsInt32())
      {
        m_frame_id = it->AsInt32();
        Tracking::TreeBuilder::update_param(&m_device->get_root_node(), "frame", m_frame_id);
      }
    }
  }
  catch (const oscpack::Exception& e)
  {
    ossia::logger().error("TUIO 2Dblb parse error: {}", e.what());
  }
}

void TUIOProtocol::process_2Dobj_set(const oscpack::ReceivedMessage& msg)
{
  auto it = msg.ArgumentsBegin();
  ++it; // Skip "set"

  if (it == msg.ArgumentsEnd() || !it->IsInt32())
    return;

  int32_t session_id = it->AsInt32();
  int slot = m_object_slots.find_or_allocate(session_id);

  auto& obj = m_object_slots.entity(slot);
  obj.session_id = session_id;
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsInt32())
    return;
  obj.class_id = it->AsInt32();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  obj.x = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  obj.y = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  obj.angle = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  obj.x_vel = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  obj.y_vel = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  obj.angle_vel = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  obj.motion_accel = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  obj.rotation_accel = it->AsFloat();

  obj.last_update = std::chrono::steady_clock::now();
  m_object_slots.mark_active(slot);

  update_object_parameters(slot, obj);
}

void TUIOProtocol::process_2Dobj_alive(const oscpack::ReceivedMessage& msg)
{
  std::vector<int32_t> alive_ids;

  auto it = msg.ArgumentsBegin();
  ++it; // Skip "alive"

  while (it != msg.ArgumentsEnd())
  {
    if (it->IsInt32())
      alive_ids.push_back(it->AsInt32());
    ++it;
  }

  process_alive_message(alive_ids, m_object_slots, "2Dobj");
}

void TUIOProtocol::process_2Dcur_set(const oscpack::ReceivedMessage& msg)
{
  auto it = msg.ArgumentsBegin();
  ++it; // Skip "set"

  if (it == msg.ArgumentsEnd() || !it->IsInt32())
    return;

  int32_t session_id = it->AsInt32();
  int slot = m_cursor_slots.find_or_allocate(session_id);

  auto& cur = m_cursor_slots.entity(slot);
  cur.session_id = session_id;
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  cur.x = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  cur.y = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  cur.x_vel = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  cur.y_vel = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  cur.motion_accel = it->AsFloat();

  cur.last_update = std::chrono::steady_clock::now();
  m_cursor_slots.mark_active(slot);

  update_cursor_parameters(slot, cur);
}

void TUIOProtocol::process_2Dcur_alive(const oscpack::ReceivedMessage& msg)
{
  std::vector<int32_t> alive_ids;

  auto it = msg.ArgumentsBegin();
  ++it; // Skip "alive"

  while (it != msg.ArgumentsEnd())
  {
    if (it->IsInt32())
      alive_ids.push_back(it->AsInt32());
    ++it;
  }

  process_alive_message(alive_ids, m_cursor_slots, "2Dcur");
}

void TUIOProtocol::process_2Dblb_set(const oscpack::ReceivedMessage& msg)
{
  auto it = msg.ArgumentsBegin();
  ++it; // Skip "set"

  if (it == msg.ArgumentsEnd() || !it->IsInt32())
    return;

  int32_t session_id = it->AsInt32();
  int slot = m_blob_slots.find_or_allocate(session_id);

  auto& blob = m_blob_slots.entity(slot);
  blob.session_id = session_id;
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  blob.x = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  blob.y = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  blob.angle = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  blob.width = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  blob.height = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  blob.area = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  blob.x_vel = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  blob.y_vel = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  blob.angle_vel = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  blob.motion_accel = it->AsFloat();
  ++it;

  if (it == msg.ArgumentsEnd() || !it->IsFloat())
    return;
  blob.rotation_accel = it->AsFloat();

  blob.last_update = std::chrono::steady_clock::now();
  m_blob_slots.mark_active(slot);

  update_blob_parameters(slot, blob);
}

void TUIOProtocol::process_2Dblb_alive(const oscpack::ReceivedMessage& msg)
{
  std::vector<int32_t> alive_ids;

  auto it = msg.ArgumentsBegin();
  ++it; // Skip "alive"

  while (it != msg.ArgumentsEnd())
  {
    if (it->IsInt32())
      alive_ids.push_back(it->AsInt32());
    ++it;
  }

  process_alive_message(alive_ids, m_blob_slots, "2Dblb");
}

template<typename SlotMgr>
void TUIOProtocol::process_alive_message(
    const std::vector<int32_t>& alive_ids,
    SlotMgr& slots,
    const std::string& node_name)
{
  if (!m_device)
    return;

  auto* profile_node = m_device->get_root_node().find_child(std::string_view(node_name));
  if (!profile_node)
    return;

  // Mark all slots as inactive
  slots.mark_all_inactive();

  // Mark alive sessions as active
  for (int32_t id : alive_ids)
  {
    slots.mark_active_by_id(id);
  }

  // Free inactive slots and update device tree
  auto freed = slots.free_inactive_slots();
  for (int slot_idx : freed)
  {
    if (auto* slot_node = profile_node->find_child(std::to_string(slot_idx)))
    {
      Tracking::TreeBuilder::update_param(slot_node, "active", false);
    }
  }
}

void TUIOProtocol::update_object_parameters(int slot, const TUIOObject& obj)
{
  if (!m_device)
    return;

  auto* obj_node = m_device->get_root_node().find_child(std::string_view("2Dobj"));
  if (!obj_node)
    return;

  auto* slot_node = obj_node->find_child(std::to_string(slot));
  if (!slot_node)
    return;

  using TB = Tracking::TreeBuilder;
  TB::update_param(slot_node, "session_id", obj.session_id);
  TB::update_param(slot_node, "active", true);
  TB::update_param(slot_node, "class_id", obj.class_id);
  TB::update_param(slot_node, "position", ossia::vec2f{obj.x, obj.y});
  TB::update_param(slot_node, "angle", obj.angle);
  TB::update_param(slot_node, "velocity", ossia::vec2f{obj.x_vel, obj.y_vel});
  TB::update_param(slot_node, "angle_velocity", obj.angle_vel);
  TB::update_param(slot_node, "motion_acceleration", obj.motion_accel);
  TB::update_param(slot_node, "rotation_acceleration", obj.rotation_accel);
}

void TUIOProtocol::update_cursor_parameters(int slot, const TUIOCursor& cur)
{
  if (!m_device)
    return;

  auto* cur_node = m_device->get_root_node().find_child(std::string_view("2Dcur"));
  if (!cur_node)
    return;

  auto* slot_node = cur_node->find_child(std::to_string(slot));
  if (!slot_node)
    return;

  using TB = Tracking::TreeBuilder;
  TB::update_param(slot_node, "session_id", cur.session_id);
  TB::update_param(slot_node, "active", true);
  TB::update_param(slot_node, "position", ossia::vec2f{cur.x, cur.y});
  TB::update_param(slot_node, "velocity", ossia::vec2f{cur.x_vel, cur.y_vel});
  TB::update_param(slot_node, "motion_acceleration", cur.motion_accel);
}

void TUIOProtocol::update_blob_parameters(int slot, const TUIOBlob& blob)
{
  if (!m_device)
    return;

  auto* blob_node = m_device->get_root_node().find_child(std::string_view("2Dblb"));
  if (!blob_node)
    return;

  auto* slot_node = blob_node->find_child(std::to_string(slot));
  if (!slot_node)
    return;

  using TB = Tracking::TreeBuilder;
  TB::update_param(slot_node, "session_id", blob.session_id);
  TB::update_param(slot_node, "active", true);
  TB::update_param(slot_node, "position", ossia::vec2f{blob.x, blob.y});
  TB::update_param(slot_node, "angle", blob.angle);
  TB::update_param(slot_node, "size", ossia::vec2f{blob.width, blob.height});
  TB::update_param(slot_node, "area", blob.area);
  TB::update_param(slot_node, "velocity", ossia::vec2f{blob.x_vel, blob.y_vel});
  TB::update_param(slot_node, "angle_velocity", blob.angle_vel);
  TB::update_param(slot_node, "motion_acceleration", blob.motion_accel);
  TB::update_param(slot_node, "rotation_acceleration", blob.rotation_accel);
}

}
