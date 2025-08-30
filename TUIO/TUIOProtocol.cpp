#include "TUIOProtocol.hpp"

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/domain/domain.hpp>

#include <fmt/format.h>
#include <unordered_set>
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
    , m_port{port}
    , m_num_objects{numObjects}
    , m_num_cursors{numCursors}
    , m_num_blobs{numBlobs}
    , m_version{version}
{
  // Initialize slot arrays
  m_object_slots.resize(numObjects);
  m_cursor_slots.resize(numCursors);
  m_blob_slots.resize(numBlobs);
  
  if (m_version == TUIOVersion::V1_1)
  {
    m_objects.resize(numObjects);
    m_cursors.resize(numCursors);
    m_blobs.resize(numBlobs);
  }
  else // TUIO 2.0
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
  
  // Create the TUIO tree structure
  auto& root = dev.get_root_node();
  
  // Create source node
  auto* source_node = root.create_child(std::string("source"));
  auto source_param = source_node->create_parameter(ossia::val_type::STRING);
  source_param->set_value("unknown");
  
  // Create frame ID node
  auto* frame_node = root.create_child(std::string("frame"));
  auto frame_param = frame_node->create_parameter(ossia::val_type::INT);
  frame_param->set_value(0);
  
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
  // Create profile nodes with fixed slots
  auto* obj_node = root.create_child(std::string("2Dobj"));
  for (int i = 0; i < m_num_objects; ++i)
  {
    auto* slot_node = obj_node->create_child(std::to_string(i));
    
    // Create session_id parameter to show which TUIO session is mapped to this slot
    slot_node->create_child(std::string("session_id"))->create_parameter(ossia::val_type::INT)->set_value(-1);
    slot_node->create_child(std::string("active"))->create_parameter(ossia::val_type::BOOL)->set_value(false);
    
    // Create all TUIO object parameters
    slot_node->create_child(std::string("class_id"))->create_parameter(ossia::val_type::INT)->set_value(0);
    
    auto* pos_param = slot_node->create_child(std::string("position"))->create_parameter(ossia::val_type::VEC2F);
    pos_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    pos_param->set_value(ossia::vec2f{0.5f, 0.5f});
    
    auto* angle_param = slot_node->create_child(std::string("angle"))->create_parameter(ossia::val_type::FLOAT);
    angle_param->set_domain(ossia::make_domain(0.0f, static_cast<float>(2 * M_PI)));
    angle_param->set_value(0.0f);
    
    slot_node->create_child(std::string("velocity"))->create_parameter(ossia::val_type::VEC2F)->set_value(ossia::vec2f{});
    slot_node->create_child(std::string("angle_velocity"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
    slot_node->create_child(std::string("motion_acceleration"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
    slot_node->create_child(std::string("rotation_acceleration"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
  }
  
  auto* cur_node = root.create_child(std::string("2Dcur"));
  for (int i = 0; i < m_num_cursors; ++i)
  {
    auto* slot_node = cur_node->create_child(std::to_string(i));
    
    slot_node->create_child(std::string("session_id"))->create_parameter(ossia::val_type::INT)->set_value(-1);
    slot_node->create_child(std::string("active"))->create_parameter(ossia::val_type::BOOL)->set_value(false);
    
    auto* pos_param = slot_node->create_child(std::string("position"))->create_parameter(ossia::val_type::VEC2F);
    pos_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    pos_param->set_value(ossia::vec2f{0.5f, 0.5f});
    
    slot_node->create_child(std::string("velocity"))->create_parameter(ossia::val_type::VEC2F)->set_value(ossia::vec2f{});
    slot_node->create_child(std::string("motion_acceleration"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
  }
  
  auto* blob_node = root.create_child(std::string("2Dblb"));
  for (int i = 0; i < m_num_blobs; ++i)
  {
    auto* slot_node = blob_node->create_child(std::to_string(i));
    
    slot_node->create_child(std::string("session_id"))->create_parameter(ossia::val_type::INT)->set_value(-1);
    slot_node->create_child(std::string("active"))->create_parameter(ossia::val_type::BOOL)->set_value(false);
    
    auto* pos_param = slot_node->create_child(std::string("position"))->create_parameter(ossia::val_type::VEC2F);
    pos_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    pos_param->set_value(ossia::vec2f{0.5f, 0.5f});
    
    auto* angle_param = slot_node->create_child(std::string("angle"))->create_parameter(ossia::val_type::FLOAT);
    angle_param->set_domain(ossia::make_domain(0.0f, static_cast<float>(2 * M_PI)));
    angle_param->set_value(0.0f);
    
    auto* size_param = slot_node->create_child(std::string("size"))->create_parameter(ossia::val_type::VEC2F);
    size_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    size_param->set_value(ossia::vec2f{0.1f, 0.1f});
    
    auto* area_param = slot_node->create_child(std::string("area"))->create_parameter(ossia::val_type::FLOAT);
    area_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    area_param->set_value(0.01f);
    
    slot_node->create_child(std::string("velocity"))->create_parameter(ossia::val_type::VEC2F)->set_value(ossia::vec2f{});
    slot_node->create_child(std::string("angle_velocity"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
    slot_node->create_child(std::string("motion_acceleration"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
    slot_node->create_child(std::string("rotation_acceleration"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
  }
}

void TUIOProtocol::create_tuio2_tree(ossia::net::node_base& root)
{
  // TUIO 2.0 uses /tuio2/tok, /tuio2/ptr, /tuio2/bnd namespaces
  // Create token nodes
  auto* tok_node = root.create_child(std::string("tok"));
  for (int i = 0; i < m_num_objects; ++i)
  {
    auto* slot_node = tok_node->create_child(std::to_string(i));
    
    slot_node->create_child(std::string("session_id"))->create_parameter(ossia::val_type::INT)->set_value(-1);
    slot_node->create_child(std::string("active"))->create_parameter(ossia::val_type::BOOL)->set_value(false);
    slot_node->create_child(std::string("user_id"))->create_parameter(ossia::val_type::INT)->set_value(0);
    slot_node->create_child(std::string("type_id"))->create_parameter(ossia::val_type::INT)->set_value(0);
    slot_node->create_child(std::string("component_id"))->create_parameter(ossia::val_type::INT)->set_value(0);
    
    auto* pos_param = slot_node->create_child(std::string("position"))->create_parameter(ossia::val_type::VEC2F);
    pos_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    pos_param->set_value(ossia::vec2f{0.5f, 0.5f});
    
    auto* angle_param = slot_node->create_child(std::string("angle"))->create_parameter(ossia::val_type::FLOAT);
    angle_param->set_domain(ossia::make_domain(0.0f, static_cast<float>(2 * M_PI)));
    angle_param->set_value(0.0f);
    
    slot_node->create_child(std::string("velocity"))->create_parameter(ossia::val_type::VEC2F)->set_value(ossia::vec2f{});
    slot_node->create_child(std::string("angle_velocity"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
    slot_node->create_child(std::string("motion_acceleration"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
    slot_node->create_child(std::string("rotation_acceleration"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
    
    // Symbol data (optional)
    slot_node->create_child(std::string("symbol_group"))->create_parameter(ossia::val_type::STRING)->set_value("");
    slot_node->create_child(std::string("symbol_data"))->create_parameter(ossia::val_type::STRING)->set_value("");
  }
  
  // Create pointer nodes
  auto* ptr_node = root.create_child(std::string("ptr"));
  for (int i = 0; i < m_num_cursors; ++i)
  {
    auto* slot_node = ptr_node->create_child(std::to_string(i));
    
    slot_node->create_child(std::string("session_id"))->create_parameter(ossia::val_type::INT)->set_value(-1);
    slot_node->create_child(std::string("active"))->create_parameter(ossia::val_type::BOOL)->set_value(false);
    slot_node->create_child(std::string("user_id"))->create_parameter(ossia::val_type::INT)->set_value(0);
    slot_node->create_child(std::string("type_id"))->create_parameter(ossia::val_type::INT)->set_value(1); // Default: right index finger
    slot_node->create_child(std::string("component_id"))->create_parameter(ossia::val_type::INT)->set_value(0);
    
    auto* pos_param = slot_node->create_child(std::string("position"))->create_parameter(ossia::val_type::VEC2F);
    pos_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    pos_param->set_value(ossia::vec2f{0.5f, 0.5f});
    
    auto* angle_param = slot_node->create_child(std::string("angle"))->create_parameter(ossia::val_type::FLOAT);
    angle_param->set_domain(ossia::make_domain(0.0f, static_cast<float>(2 * M_PI)));
    angle_param->set_value(0.0f);
    
    auto* shear_param = slot_node->create_child(std::string("shear"))->create_parameter(ossia::val_type::FLOAT);
    shear_param->set_domain(ossia::make_domain(0.0f, static_cast<float>(M_PI/2)));
    shear_param->set_value(0.0f);
    
    auto* radius_param = slot_node->create_child(std::string("radius"))->create_parameter(ossia::val_type::FLOAT);
    radius_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    radius_param->set_value(0.0f);
    
    auto* pressure_param = slot_node->create_child(std::string("pressure"))->create_parameter(ossia::val_type::FLOAT);
    pressure_param->set_domain(ossia::make_domain(-1.0f, 1.0f)); // -1 = hovering
    pressure_param->set_value(1.0f);
    
    slot_node->create_child(std::string("velocity"))->create_parameter(ossia::val_type::VEC2F)->set_value(ossia::vec2f{});
    slot_node->create_child(std::string("pressure_velocity"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
    slot_node->create_child(std::string("motion_acceleration"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
    slot_node->create_child(std::string("pressure_acceleration"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
  }
  
  // Create bounds nodes
  auto* bnd_node = root.create_child(std::string("bnd"));
  for (int i = 0; i < m_num_blobs; ++i)
  {
    auto* slot_node = bnd_node->create_child(std::to_string(i));
    
    slot_node->create_child(std::string("session_id"))->create_parameter(ossia::val_type::INT)->set_value(-1);
    slot_node->create_child(std::string("active"))->create_parameter(ossia::val_type::BOOL)->set_value(false);
    
    auto* pos_param = slot_node->create_child(std::string("position"))->create_parameter(ossia::val_type::VEC2F);
    pos_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    pos_param->set_value(ossia::vec2f{0.5f, 0.5f});
    
    auto* angle_param = slot_node->create_child(std::string("angle"))->create_parameter(ossia::val_type::FLOAT);
    angle_param->set_domain(ossia::make_domain(0.0f, static_cast<float>(2 * M_PI)));
    angle_param->set_value(0.0f);
    
    auto* size_param = slot_node->create_child(std::string("size"))->create_parameter(ossia::val_type::VEC2F);
    size_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    size_param->set_value(ossia::vec2f{0.1f, 0.1f});
    
    auto* area_param = slot_node->create_child(std::string("area"))->create_parameter(ossia::val_type::FLOAT);
    area_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    area_param->set_value(0.01f);
    
    slot_node->create_child(std::string("velocity"))->create_parameter(ossia::val_type::VEC2F)->set_value(ossia::vec2f{});
    slot_node->create_child(std::string("angle_velocity"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
    slot_node->create_child(std::string("motion_acceleration"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
    slot_node->create_child(std::string("rotation_acceleration"))->create_parameter(ossia::val_type::FLOAT)->set_value(0.0f);
  }
}

int TUIOProtocol::find_or_allocate_slot(int32_t session_id, std::vector<SlotMapping>& slots, 
                                        ossia::hash_map<int32_t, int>& session_map)
{
  // Check if session already has a slot
  auto it = session_map.find(session_id);
  if (it != session_map.end())
  {
    return it->second;
  }
  
  // Find first free slot (round-robin)
  for (size_t i = 0; i < slots.size(); ++i)
  {
    if (slots[i].session_id == -1)
    {
      slots[i].session_id = session_id;
      slots[i].active = true;
      session_map[session_id] = i;
      return i;
    }
  }
  
  // No free slot available - reuse oldest inactive slot
  // In a real implementation, you might want to handle this differently
  for (size_t i = 0; i < slots.size(); ++i)
  {
    if (!slots[i].active)
    {
      // Remove old mapping
      if (slots[i].session_id != -1)
      {
        session_map.erase(slots[i].session_id);
      }
      
      slots[i].session_id = session_id;
      slots[i].active = true;
      session_map[session_id] = i;
      return i;
    }
  }
  
  // All slots are active - this shouldn't happen if ALIVE messages are handled correctly
  // For now, just use slot 0
  return 0;
}

void TUIOProtocol::free_slot(int32_t session_id, std::vector<SlotMapping>& slots,
                             ossia::hash_map<int32_t, int>& session_map)
{
  auto it = session_map.find(session_id);
  if (it != session_map.end())
  {
    int slot = it->second;
    slots[slot].session_id = -1;
    slots[slot].active = false;
    session_map.erase(it);
  }
}

void TUIOProtocol::setup_receive_socket()
{
  try
  {
    m_receive_socket = std::make_unique<ossia::net::udp_receive_socket>(
        ossia::net::inbound_socket_configuration{
            .port = m_port
        },
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
    // TUIO 1.1 messages
    if (addr == "/tuio/2Dobj")
    {
      handle_2Dobj_message(msg);
    }
    else if (addr == "/tuio/2Dcur")
    {
      handle_2Dcur_message(msg);
    }
    else if (addr == "/tuio/2Dblb")
    {
      handle_2Dblb_message(msg);
    }
  }
  else // TUIO 2.0
  {
    // TUIO 2.0 messages
    if (addr == "/tuio2/frm")
    {
      handle_tuio2_frm(msg);
    }
    else if (addr == "/tuio2/tok")
    {
      handle_tuio2_tok(msg);
    }
    else if (addr == "/tuio2/ptr")
    {
      handle_tuio2_ptr(msg);
    }
    else if (addr == "/tuio2/bnd")
    {
      handle_tuio2_bnd(msg);
    }
    else if (addr == "/tuio2/sym")
    {
      handle_tuio2_sym(msg);
    }
    else if (addr == "/tuio2/ctl")
    {
      handle_tuio2_ctl(msg);
    }
    else if (addr == "/tuio2/alv")
    {
      handle_tuio2_alv(msg);
    }
  }
}

void TUIOProtocol::handle_2Dobj_message(const oscpack::ReceivedMessage& msg)
{
  try
  {
    auto it = msg.ArgumentsBegin();
    if (it == msg.ArgumentsEnd())
      return;
    
    if (!it->IsString())
      return;
    
    std::string cmd = it->AsString();
    
    if (cmd == "source")
    {
      ++it;
      if (it != msg.ArgumentsEnd() && it->IsString())
      {
        m_source = it->AsString();
        if (auto* source_node = m_device->get_root_node().find_child(std::string_view("source")))
        {
          if (auto* param = source_node->get_parameter())
          {
            param->set_value(m_source);
          }
        }
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
        if (auto* frame_node = m_device->get_root_node().find_child(std::string_view("frame")))
        {
          if (auto* param = frame_node->get_parameter())
          {
            param->set_value(m_frame_id);
          }
        }
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
    if (it == msg.ArgumentsEnd())
      return;
    
    if (!it->IsString())
      return;
    
    std::string cmd = it->AsString();
    
    if (cmd == "source")
    {
      ++it;
      if (it != msg.ArgumentsEnd() && it->IsString())
      {
        m_source = it->AsString();
        if (auto* source_node = m_device->get_root_node().find_child(std::string_view("source")))
        {
          if (auto* param = source_node->get_parameter())
          {
            param->set_value(m_source);
          }
        }
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
        if (auto* frame_node = m_device->get_root_node().find_child(std::string_view("frame")))
        {
          if (auto* param = frame_node->get_parameter())
          {
            param->set_value(m_frame_id);
          }
        }
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
    if (it == msg.ArgumentsEnd())
      return;
    
    if (!it->IsString())
      return;
    
    std::string cmd = it->AsString();
    
    if (cmd == "source")
    {
      ++it;
      if (it != msg.ArgumentsEnd() && it->IsString())
      {
        m_source = it->AsString();
        if (auto* source_node = m_device->get_root_node().find_child(std::string_view("source")))
        {
          if (auto* param = source_node->get_parameter())
          {
            param->set_value(m_source);
          }
        }
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
        if (auto* frame_node = m_device->get_root_node().find_child(std::string_view("frame")))
        {
          if (auto* param = frame_node->get_parameter())
          {
            param->set_value(m_frame_id);
          }
        }
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
  int slot = find_or_allocate_slot(session_id, m_object_slots, m_object_session_to_slot);
  
  TUIOObject& obj = m_objects[slot];
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
  m_object_slots[slot].active = true;
  
  update_object_parameters(session_id, obj);
}

void TUIOProtocol::process_2Dobj_alive(const oscpack::ReceivedMessage& msg)
{
  std::vector<int32_t> alive_ids;
  
  auto it = msg.ArgumentsBegin();
  ++it; // Skip "alive"
  
  while (it != msg.ArgumentsEnd())
  {
    if (it->IsInt32())
    {
      alive_ids.push_back(it->AsInt32());
    }
    ++it;
  }
  
  remove_dead_sessions(alive_ids, "2Dobj");
}

void TUIOProtocol::process_2Dcur_set(const oscpack::ReceivedMessage& msg)
{
  auto it = msg.ArgumentsBegin();
  ++it; // Skip "set"
  
  if (it == msg.ArgumentsEnd() || !it->IsInt32())
    return;
  
  int32_t session_id = it->AsInt32();
  int slot = find_or_allocate_slot(session_id, m_cursor_slots, m_cursor_session_to_slot);
  
  TUIOCursor& cur = m_cursors[slot];
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
  m_cursor_slots[slot].active = true;
  
  update_cursor_parameters(session_id, cur);
}

void TUIOProtocol::process_2Dcur_alive(const oscpack::ReceivedMessage& msg)
{
  std::vector<int32_t> alive_ids;
  
  auto it = msg.ArgumentsBegin();
  ++it; // Skip "alive"
  
  while (it != msg.ArgumentsEnd())
  {
    if (it->IsInt32())
    {
      alive_ids.push_back(it->AsInt32());
    }
    ++it;
  }
  
  remove_dead_sessions(alive_ids, "2Dcur");
}

void TUIOProtocol::process_2Dblb_set(const oscpack::ReceivedMessage& msg)
{
  auto it = msg.ArgumentsBegin();
  ++it; // Skip "set"
  
  if (it == msg.ArgumentsEnd() || !it->IsInt32())
    return;
  
  int32_t session_id = it->AsInt32();
  int slot = find_or_allocate_slot(session_id, m_blob_slots, m_blob_session_to_slot);
  
  TUIOBlob& blob = m_blobs[slot];
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
  m_blob_slots[slot].active = true;
  
  update_blob_parameters(session_id, blob);
}

void TUIOProtocol::process_2Dblb_alive(const oscpack::ReceivedMessage& msg)
{
  std::vector<int32_t> alive_ids;
  
  auto it = msg.ArgumentsBegin();
  ++it; // Skip "alive"
  
  while (it != msg.ArgumentsEnd())
  {
    if (it->IsInt32())
    {
      alive_ids.push_back(it->AsInt32());
    }
    ++it;
  }
  
  remove_dead_sessions(alive_ids, "2Dblb");
}

void TUIOProtocol::update_object_parameters(int32_t session_id, const TUIOObject& obj)
{
  if (!m_device)
    return;
  
  auto it = m_object_session_to_slot.find(session_id);
  if (it == m_object_session_to_slot.end())
    return;
  
  int slot = it->second;
  
  auto* obj_node = m_device->get_root_node().find_child(std::string_view("2Dobj"));
  if (!obj_node)
    return;
  
  auto* slot_node = obj_node->find_child(std::to_string(slot));
  if (!slot_node)
    return;
  
  // Update session_id and active status
  if (auto* param = slot_node->find_child(std::string_view("session_id"))->get_parameter())
    param->set_value(session_id);
  if (auto* param = slot_node->find_child(std::string_view("active"))->get_parameter())
    param->set_value(true);
  
  // Update object parameters
  if (auto* param = slot_node->find_child(std::string_view("class_id"))->get_parameter())
    param->set_value(obj.class_id);
  
  if (auto* param = slot_node->find_child(std::string_view("position"))->get_parameter())
    param->set_value(ossia::vec2f{obj.x, obj.y});
  
  if (auto* param = slot_node->find_child(std::string_view("angle"))->get_parameter())
    param->set_value(obj.angle);
  
  if (auto* param = slot_node->find_child(std::string_view("velocity"))->get_parameter())
    param->set_value(ossia::vec2f{obj.x_vel, obj.y_vel});
  
  if (auto* param = slot_node->find_child(std::string_view("angle_velocity"))->get_parameter())
    param->set_value(obj.angle_vel);
  
  if (auto* param = slot_node->find_child(std::string_view("motion_acceleration"))->get_parameter())
    param->set_value(obj.motion_accel);
  
  if (auto* param = slot_node->find_child(std::string_view("rotation_acceleration"))->get_parameter())
    param->set_value(obj.rotation_accel);
}

void TUIOProtocol::update_cursor_parameters(int32_t session_id, const TUIOCursor& cur)
{
  if (!m_device)
    return;
  
  auto it = m_cursor_session_to_slot.find(session_id);
  if (it == m_cursor_session_to_slot.end())
    return;
  
  int slot = it->second;
  
  auto* cur_node = m_device->get_root_node().find_child(std::string_view("2Dcur"));
  if (!cur_node)
    return;
  
  auto* slot_node = cur_node->find_child(std::to_string(slot));
  if (!slot_node)
    return;
  
  // Update session_id and active status
  if (auto* param = slot_node->find_child(std::string_view("session_id"))->get_parameter())
    param->set_value(session_id);
  if (auto* param = slot_node->find_child(std::string_view("active"))->get_parameter())
    param->set_value(true);
  
  // Update cursor parameters
  if (auto* param = slot_node->find_child(std::string_view("position"))->get_parameter())
    param->set_value(ossia::vec2f{cur.x, cur.y});
  
  if (auto* param = slot_node->find_child(std::string_view("velocity"))->get_parameter())
    param->set_value(ossia::vec2f{cur.x_vel, cur.y_vel});
  
  if (auto* param = slot_node->find_child(std::string_view("motion_acceleration"))->get_parameter())
    param->set_value(cur.motion_accel);
}

void TUIOProtocol::update_blob_parameters(int32_t session_id, const TUIOBlob& blob)
{
  if (!m_device)
    return;
  
  auto it = m_blob_session_to_slot.find(session_id);
  if (it == m_blob_session_to_slot.end())
    return;
  
  int slot = it->second;
  
  auto* blob_node = m_device->get_root_node().find_child(std::string_view("2Dblb"));
  if (!blob_node)
    return;
  
  auto* slot_node = blob_node->find_child(std::to_string(slot));
  if (!slot_node)
    return;
  
  // Update session_id and active status
  if (auto* param = slot_node->find_child(std::string_view("session_id"))->get_parameter())
    param->set_value(session_id);
  if (auto* param = slot_node->find_child(std::string_view("active"))->get_parameter())
    param->set_value(true);
  
  // Update blob parameters
  if (auto* param = slot_node->find_child(std::string_view("position"))->get_parameter())
    param->set_value(ossia::vec2f{blob.x, blob.y});
  
  if (auto* param = slot_node->find_child(std::string_view("angle"))->get_parameter())
    param->set_value(blob.angle);
  
  if (auto* param = slot_node->find_child(std::string_view("size"))->get_parameter())
    param->set_value(ossia::vec2f{blob.width, blob.height});
  
  if (auto* param = slot_node->find_child(std::string_view("area"))->get_parameter())
    param->set_value(blob.area);
  
  if (auto* param = slot_node->find_child(std::string_view("velocity"))->get_parameter())
    param->set_value(ossia::vec2f{blob.x_vel, blob.y_vel});
  
  if (auto* param = slot_node->find_child(std::string_view("angle_velocity"))->get_parameter())
    param->set_value(blob.angle_vel);
  
  if (auto* param = slot_node->find_child(std::string_view("motion_acceleration"))->get_parameter())
    param->set_value(blob.motion_accel);
  
  if (auto* param = slot_node->find_child(std::string_view("rotation_acceleration"))->get_parameter())
    param->set_value(blob.rotation_accel);
}

void TUIOProtocol::remove_dead_sessions(const std::vector<int32_t>& alive_ids, const std::string& profile)
{
  if (!m_device)
    return;
  
  auto* profile_node = m_device->get_root_node().find_child(std::string_view(profile));
  if (!profile_node)
    return;
  
  // Build set of alive IDs for fast lookup
  std::unordered_set<int32_t> alive_set(alive_ids.begin(), alive_ids.end());
  
  // Mark slots as inactive for dead sessions
  if (profile == "2Dobj")
  {
    // First mark all slots as inactive
    for (auto& slot : m_object_slots)
    {
      slot.active = false;
    }
    
    // Then mark alive sessions as active
    for (int32_t id : alive_ids)
    {
      auto it = m_object_session_to_slot.find(id);
      if (it != m_object_session_to_slot.end())
      {
        m_object_slots[it->second].active = true;
      }
    }
    
    // Update device tree for inactive slots
    for (size_t i = 0; i < m_object_slots.size(); ++i)
    {
      if (!m_object_slots[i].active && m_object_slots[i].session_id != -1)
      {
        auto* slot_node = profile_node->find_child(std::to_string(i));
        if (slot_node)
        {
          if (auto* param = slot_node->find_child(std::string_view("active"))->get_parameter())
            param->set_value(false);
        }
        
        // Free the slot
        m_object_session_to_slot.erase(m_object_slots[i].session_id);
        m_object_slots[i].session_id = -1;
      }
    }
  }
  else if (profile == "2Dcur")
  {
    for (auto& slot : m_cursor_slots)
    {
      slot.active = false;
    }
    
    for (int32_t id : alive_ids)
    {
      auto it = m_cursor_session_to_slot.find(id);
      if (it != m_cursor_session_to_slot.end())
      {
        m_cursor_slots[it->second].active = true;
      }
    }
    
    for (size_t i = 0; i < m_cursor_slots.size(); ++i)
    {
      if (!m_cursor_slots[i].active && m_cursor_slots[i].session_id != -1)
      {
        auto* slot_node = profile_node->find_child(std::to_string(i));
        if (slot_node)
        {
          if (auto* param = slot_node->find_child(std::string_view("active"))->get_parameter())
            param->set_value(false);
        }
        
        m_cursor_session_to_slot.erase(m_cursor_slots[i].session_id);
        m_cursor_slots[i].session_id = -1;
      }
    }
  }
  else if (profile == "2Dblb")
  {
    for (auto& slot : m_blob_slots)
    {
      slot.active = false;
    }
    
    for (int32_t id : alive_ids)
    {
      auto it = m_blob_session_to_slot.find(id);
      if (it != m_blob_session_to_slot.end())
      {
        m_blob_slots[it->second].active = true;
      }
    }
    
    for (size_t i = 0; i < m_blob_slots.size(); ++i)
    {
      if (!m_blob_slots[i].active && m_blob_slots[i].session_id != -1)
      {
        auto* slot_node = profile_node->find_child(std::to_string(i));
        if (slot_node)
        {
          if (auto* param = slot_node->find_child(std::string_view("active"))->get_parameter())
            param->set_value(false);
        }
        
        m_blob_session_to_slot.erase(m_blob_slots[i].session_id);
        m_blob_slots[i].session_id = -1;
      }
    }
  }
}

}