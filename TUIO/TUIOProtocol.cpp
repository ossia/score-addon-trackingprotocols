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
    uint16_t port)
    : ossia::net::protocol_base{}
    , m_ctx{ctx}
    , m_port{port}
{
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
  
  // Create profile nodes
  root.create_child(std::string("2Dobj"));
  root.create_child(std::string("2Dcur"));
  root.create_child(std::string("2Dblb"));
  
  setup_receive_socket();
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
  // 2.5D and 3D profiles can be added later
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
  
  TUIOObject obj;
  obj.session_id = it->AsInt32();
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
  m_objects[obj.session_id] = obj;
  update_object_parameters(obj.session_id, obj);
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
  
  TUIOCursor cur;
  cur.session_id = it->AsInt32();
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
  m_cursors[cur.session_id] = cur;
  update_cursor_parameters(cur.session_id, cur);
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
  
  TUIOBlob blob;
  blob.session_id = it->AsInt32();
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
  m_blobs[blob.session_id] = blob;
  update_blob_parameters(blob.session_id, blob);
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
  
  auto* obj_node = m_device->get_root_node().find_child(std::string_view("2Dobj"));
  if (!obj_node)
    return;
  
  std::string session_name = std::to_string(session_id);
  auto* session_node = obj_node->find_child(session_name);
  
  if (!session_node)
  {
    session_node = obj_node->create_child(session_name);
    
    // Create all parameters for this object
    auto* class_param = session_node->create_child(std::string("class_id"))->create_parameter(ossia::val_type::INT);
    class_param->set_value(obj.class_id);
    
    auto* pos_param = session_node->create_child(std::string("position"))->create_parameter(ossia::val_type::VEC2F);
    pos_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    
    auto* angle_param = session_node->create_child(std::string("angle"))->create_parameter(ossia::val_type::FLOAT);
    angle_param->set_domain(ossia::make_domain(0.0f, static_cast<float>(2 * M_PI)));
    
    session_node->create_child(std::string("velocity"))->create_parameter(ossia::val_type::VEC2F);
    session_node->create_child(std::string("angle_velocity"))->create_parameter(ossia::val_type::FLOAT);
    session_node->create_child(std::string("motion_acceleration"))->create_parameter(ossia::val_type::FLOAT);
    session_node->create_child(std::string("rotation_acceleration"))->create_parameter(ossia::val_type::FLOAT);
  }
  
  // Update parameters
  if (auto* param = session_node->find_child(std::string_view("class_id"))->get_parameter())
    param->set_value(obj.class_id);
  
  if (auto* param = session_node->find_child(std::string_view("position"))->get_parameter())
    param->set_value(ossia::vec2f{obj.x, obj.y});
  
  if (auto* param = session_node->find_child(std::string_view("angle"))->get_parameter())
    param->set_value(obj.angle);
  
  if (auto* param = session_node->find_child(std::string_view("velocity"))->get_parameter())
    param->set_value(ossia::vec2f{obj.x_vel, obj.y_vel});
  
  if (auto* param = session_node->find_child(std::string_view("angle_velocity"))->get_parameter())
    param->set_value(obj.angle_vel);
  
  if (auto* param = session_node->find_child(std::string_view("motion_acceleration"))->get_parameter())
    param->set_value(obj.motion_accel);
  
  if (auto* param = session_node->find_child(std::string_view("rotation_acceleration"))->get_parameter())
    param->set_value(obj.rotation_accel);
}

void TUIOProtocol::update_cursor_parameters(int32_t session_id, const TUIOCursor& cur)
{
  if (!m_device)
    return;
  
  auto* cur_node = m_device->get_root_node().find_child(std::string_view("2Dcur"));
  if (!cur_node)
    return;
  
  std::string session_name = std::to_string(session_id);
  auto* session_node = cur_node->find_child(session_name);
  
  if (!session_node)
  {
    session_node = cur_node->create_child(session_name);
    
    // Create all parameters for this cursor
    auto* pos_param = session_node->create_child(std::string("position"))->create_parameter(ossia::val_type::VEC2F);
    pos_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    
    session_node->create_child(std::string("velocity"))->create_parameter(ossia::val_type::VEC2F);
    session_node->create_child(std::string("motion_acceleration"))->create_parameter(ossia::val_type::FLOAT);
  }
  
  // Update parameters
  if (auto* param = session_node->find_child(std::string_view("position"))->get_parameter())
    param->set_value(ossia::vec2f{cur.x, cur.y});
  
  if (auto* param = session_node->find_child(std::string_view("velocity"))->get_parameter())
    param->set_value(ossia::vec2f{cur.x_vel, cur.y_vel});
  
  if (auto* param = session_node->find_child(std::string_view("motion_acceleration"))->get_parameter())
    param->set_value(cur.motion_accel);
}

void TUIOProtocol::update_blob_parameters(int32_t session_id, const TUIOBlob& blob)
{
  if (!m_device)
    return;
  
  auto* blob_node = m_device->get_root_node().find_child(std::string_view("2Dblb"));
  if (!blob_node)
    return;
  
  std::string session_name = std::to_string(session_id);
  auto* session_node = blob_node->find_child(session_name);
  
  if (!session_node)
  {
    session_node = blob_node->create_child(session_name);
    
    // Create all parameters for this blob
    auto* pos_param = session_node->create_child(std::string("position"))->create_parameter(ossia::val_type::VEC2F);
    pos_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    
    auto* angle_param = session_node->create_child(std::string("angle"))->create_parameter(ossia::val_type::FLOAT);
    angle_param->set_domain(ossia::make_domain(0.0f, static_cast<float>(2 * M_PI)));
    
    auto* size_param = session_node->create_child(std::string("size"))->create_parameter(ossia::val_type::VEC2F);
    size_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    
    auto* area_param = session_node->create_child(std::string("area"))->create_parameter(ossia::val_type::FLOAT);
    area_param->set_domain(ossia::make_domain(0.0f, 1.0f));
    
    session_node->create_child(std::string("velocity"))->create_parameter(ossia::val_type::VEC2F);
    session_node->create_child(std::string("angle_velocity"))->create_parameter(ossia::val_type::FLOAT);
    session_node->create_child(std::string("motion_acceleration"))->create_parameter(ossia::val_type::FLOAT);
    session_node->create_child(std::string("rotation_acceleration"))->create_parameter(ossia::val_type::FLOAT);
  }
  
  // Update parameters
  if (auto* param = session_node->find_child(std::string_view("position"))->get_parameter())
    param->set_value(ossia::vec2f{blob.x, blob.y});
  
  if (auto* param = session_node->find_child(std::string_view("angle"))->get_parameter())
    param->set_value(blob.angle);
  
  if (auto* param = session_node->find_child(std::string_view("size"))->get_parameter())
    param->set_value(ossia::vec2f{blob.width, blob.height});
  
  if (auto* param = session_node->find_child(std::string_view("area"))->get_parameter())
    param->set_value(blob.area);
  
  if (auto* param = session_node->find_child(std::string_view("velocity"))->get_parameter())
    param->set_value(ossia::vec2f{blob.x_vel, blob.y_vel});
  
  if (auto* param = session_node->find_child(std::string_view("angle_velocity"))->get_parameter())
    param->set_value(blob.angle_vel);
  
  if (auto* param = session_node->find_child(std::string_view("motion_acceleration"))->get_parameter())
    param->set_value(blob.motion_accel);
  
  if (auto* param = session_node->find_child(std::string_view("rotation_acceleration"))->get_parameter())
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
  
  // Remove dead sessions based on profile
  if (profile == "2Dobj")
  {
    for (auto it = m_objects.begin(); it != m_objects.end();)
    {
      if (alive_set.find(it->first) == alive_set.end())
      {
        // Remove from device tree
        std::string session_name = std::to_string(it->first);
        if (auto* session_node = profile_node->find_child(session_name))
        {
          profile_node->remove_child(*session_node);
        }
        
        // Remove from internal storage
        it = m_objects.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }
  else if (profile == "2Dcur")
  {
    for (auto it = m_cursors.begin(); it != m_cursors.end();)
    {
      if (alive_set.find(it->first) == alive_set.end())
      {
        // Remove from device tree
        std::string session_name = std::to_string(it->first);
        if (auto* session_node = profile_node->find_child(session_name))
        {
          profile_node->remove_child(*session_node);
        }
        
        // Remove from internal storage
        it = m_cursors.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }
  else if (profile == "2Dblb")
  {
    for (auto it = m_blobs.begin(); it != m_blobs.end();)
    {
      if (alive_set.find(it->first) == alive_set.end())
      {
        // Remove from device tree
        std::string session_name = std::to_string(it->first);
        if (auto* session_node = profile_node->find_child(session_name))
        {
          profile_node->remove_child(*session_node);
        }
        
        // Remove from internal storage
        it = m_blobs.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }
}

}