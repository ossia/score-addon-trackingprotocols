#include "TUIOProtocol.hpp"
#include "TUIO2Types.hpp"

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>

#include <fmt/format.h>

namespace TUIO
{

void TUIOProtocol::handle_tuio2_frm(const oscpack::ReceivedMessage& msg)
{
  try
  {
    auto args = msg.ArgumentsBegin();
    
    // Parse: f_id time dim source
    m_tuio2_frame_id = (args++)->AsInt32();
    
    // Skip time tag for now (complex to parse)
    args++;
    
    // Parse dimension (width and height encoded in single int32)
    uint32_t dim = (args++)->AsInt32();
    m_tuio2_dim_width = (dim >> 16) & 0xFFFF;
    m_tuio2_dim_height = dim & 0xFFFF;
    
    m_tuio2_source = (args++)->AsString();
    
    // Update source and frame parameters
    if (m_device)
    {
      auto& root = m_device->get_root_node();
      if (auto* source_node = root.find_child(std::string_view("source")))
      {
        if (auto* param = source_node->get_parameter())
          param->set_value(m_tuio2_source);
      }
      if (auto* frame_node = root.find_child(std::string_view("frame")))
      {
        if (auto* param = frame_node->get_parameter())
          param->set_value((int)m_tuio2_frame_id);
      }
    }
  }
  catch (const std::exception& e)
  {
    // Invalid message
  }
}

void TUIOProtocol::handle_tuio2_tok(const oscpack::ReceivedMessage& msg)
{
  try
  {
    auto args = msg.ArgumentsBegin();
    
    // Parse: s_id tu_id c_id x_pos y_pos angle [x_vel y_vel a_vel m_acc r_acc]
    uint32_t session_id = (args++)->AsInt32();
    uint32_t tu_id = (args++)->AsInt32();
    uint32_t component_id = (args++)->AsInt32();
    float x = (args++)->AsFloat();
    float y = (args++)->AsFloat();
    float angle = (args++)->AsFloat();
    
    // Find or allocate slot
    int slot = find_or_allocate_slot(session_id, m_object_slots, m_object_session_to_slot);
    
    // Update data
    auto& tok = m_tuio2_tokens[slot];
    tok.session_id = session_id;
    tok.user_id = (tu_id >> 16) & 0xFFFF;
    tok.type_id = tu_id & 0xFFFF;
    tok.component_id = component_id;
    tok.x = x;
    tok.y = y;
    tok.angle = angle;
    
    // Parse optional velocity/acceleration
    if (args != msg.ArgumentsEnd())
    {
      tok.x_vel = (args++)->AsFloat();
      tok.y_vel = (args++)->AsFloat();
      tok.a_vel = (args++)->AsFloat();
      tok.m_acc = (args++)->AsFloat();
      tok.r_acc = (args++)->AsFloat();
    }
    
    tok.last_update = std::chrono::steady_clock::now();
    
    // Update parameters
    update_tuio2_token(slot, tok);
  }
  catch (const std::exception& e)
  {
    // Invalid message
  }
}

void TUIOProtocol::handle_tuio2_ptr(const oscpack::ReceivedMessage& msg)
{
  try
  {
    auto args = msg.ArgumentsBegin();
    
    // Parse: s_id tu_id c_id x_pos y_pos angle shear radius press [x_vel y_vel p_vel m_acc p_acc]
    uint32_t session_id = (args++)->AsInt32();
    uint32_t tu_id = (args++)->AsInt32();
    uint32_t component_id = (args++)->AsInt32();
    float x = (args++)->AsFloat();
    float y = (args++)->AsFloat();
    float angle = (args++)->AsFloat();
    float shear = (args++)->AsFloat();
    float radius = (args++)->AsFloat();
    float pressure = (args++)->AsFloat();
    
    // Find or allocate slot
    int slot = find_or_allocate_slot(session_id, m_cursor_slots, m_cursor_session_to_slot);
    
    // Update data
    auto& ptr = m_tuio2_pointers[slot];
    ptr.session_id = session_id;
    ptr.user_id = (tu_id >> 16) & 0xFFFF;
    ptr.type_id = tu_id & 0xFFFF;
    ptr.component_id = component_id;
    ptr.x = x;
    ptr.y = y;
    ptr.angle = angle;
    ptr.shear = shear;
    ptr.radius = radius;
    ptr.pressure = pressure;
    
    // Parse optional velocity/acceleration
    if (args != msg.ArgumentsEnd())
    {
      ptr.x_vel = (args++)->AsFloat();
      ptr.y_vel = (args++)->AsFloat();
      ptr.p_vel = (args++)->AsFloat();
      ptr.m_acc = (args++)->AsFloat();
      ptr.p_acc = (args++)->AsFloat();
    }
    
    ptr.last_update = std::chrono::steady_clock::now();
    
    // Update parameters
    update_tuio2_pointer(slot, ptr);
  }
  catch (const std::exception& e)
  {
    // Invalid message
  }
}

void TUIOProtocol::handle_tuio2_bnd(const oscpack::ReceivedMessage& msg)
{
  try
  {
    auto args = msg.ArgumentsBegin();
    
    // Parse: s_id x_pos y_pos angle width height area [x_vel y_vel a_vel m_acc r_acc]
    uint32_t session_id = (args++)->AsInt32();
    float x = (args++)->AsFloat();
    float y = (args++)->AsFloat();
    float angle = (args++)->AsFloat();
    float width = (args++)->AsFloat();
    float height = (args++)->AsFloat();
    float area = (args++)->AsFloat();
    
    // Find or allocate slot
    int slot = find_or_allocate_slot(session_id, m_blob_slots, m_blob_session_to_slot);
    
    // Update data
    auto& bnd = m_tuio2_bounds[slot];
    bnd.session_id = session_id;
    bnd.x = x;
    bnd.y = y;
    bnd.angle = angle;
    bnd.width = width;
    bnd.height = height;
    bnd.area = area;
    
    // Parse optional velocity/acceleration
    if (args != msg.ArgumentsEnd())
    {
      bnd.x_vel = (args++)->AsFloat();
      bnd.y_vel = (args++)->AsFloat();
      bnd.a_vel = (args++)->AsFloat();
      bnd.m_acc = (args++)->AsFloat();
      bnd.r_acc = (args++)->AsFloat();
    }
    
    bnd.last_update = std::chrono::steady_clock::now();
    
    // Update parameters
    update_tuio2_bounds(slot, bnd);
  }
  catch (const std::exception& e)
  {
    // Invalid message
  }
}

void TUIOProtocol::handle_tuio2_sym(const oscpack::ReceivedMessage& msg)
{
  try
  {
    auto args = msg.ArgumentsBegin();
    
    // Parse: s_id tu_id c_id group data
    uint32_t session_id = (args++)->AsInt32();
    uint32_t tu_id = (args++)->AsInt32();
    uint32_t component_id = (args++)->AsInt32();
    std::string group = (args++)->AsString();
    std::string data = (args++)->AsString();
    
    // Find the slot for this session
    auto it = m_object_session_to_slot.find(session_id);
    if (it != m_object_session_to_slot.end())
    {
      int slot = it->second;
      auto& sym = m_tuio2_symbols[slot];
      sym.session_id = session_id;
      sym.user_id = (tu_id >> 16) & 0xFFFF;
      sym.type_id = tu_id & 0xFFFF;
      sym.component_id = component_id;
      sym.group = group;
      sym.data = data;
      
      // Update symbol parameters
      if (m_device)
      {
        auto& root = m_device->get_root_node();
        if (auto* tok_node = root.find_child(std::string_view("tok")))
        {
          if (auto* slot_node = tok_node->find_child(std::to_string(slot)))
          {
            if (auto* param = slot_node->find_child(std::string_view("symbol_group"))->get_parameter())
              param->set_value(group);
            if (auto* param = slot_node->find_child(std::string_view("symbol_data"))->get_parameter())
              param->set_value(data);
          }
        }
      }
    }
  }
  catch (const std::exception& e)
  {
    // Invalid message
  }
}

void TUIOProtocol::handle_tuio2_ctl(const oscpack::ReceivedMessage& msg)
{
  try
  {
    auto args = msg.ArgumentsBegin();
    
    // Parse: s_id c0 ... cN
    uint32_t session_id = (args++)->AsInt32();
    
    // Find the slot for this session (could be in any profile)
    auto it = m_object_session_to_slot.find(session_id);
    if (it != m_object_session_to_slot.end())
    {
      int slot = it->second;
      auto& ctl = m_tuio2_controls[slot];
      ctl.session_id = session_id;
      ctl.values.clear();
      
      // Parse all control values
      while (args != msg.ArgumentsEnd())
      {
        if (args->IsBool())
          ctl.values.push_back(args->AsBool() ? 1.0f : 0.0f);
        else if (args->IsFloat())
          ctl.values.push_back(args->AsFloat());
        else if (args->IsInt32())
          ctl.values.push_back((float)args->AsInt32());
        args++;
      }
      
      // Could update control parameters here if needed
    }
  }
  catch (const std::exception& e)
  {
    // Invalid message
  }
}

void TUIOProtocol::handle_tuio2_alv(const oscpack::ReceivedMessage& msg)
{
  try
  {
    std::vector<uint32_t> alive_ids;
    
    auto args = msg.ArgumentsBegin();
    while (args != msg.ArgumentsEnd())
    {
      alive_ids.push_back((args++)->AsInt32());
    }
    
    // Update active status for all profiles
    // For tokens
    for (auto& slot : m_object_slots)
    {
      slot.active = false;
    }
    for (uint32_t id : alive_ids)
    {
      auto it = m_object_session_to_slot.find(id);
      if (it != m_object_session_to_slot.end())
      {
        m_object_slots[it->second].active = true;
      }
    }
    
    // For pointers
    for (auto& slot : m_cursor_slots)
    {
      slot.active = false;
    }
    for (uint32_t id : alive_ids)
    {
      auto it = m_cursor_session_to_slot.find(id);
      if (it != m_cursor_session_to_slot.end())
      {
        m_cursor_slots[it->second].active = true;
      }
    }
    
    // For bounds
    for (auto& slot : m_blob_slots)
    {
      slot.active = false;
    }
    for (uint32_t id : alive_ids)
    {
      auto it = m_blob_session_to_slot.find(id);
      if (it != m_blob_session_to_slot.end())
      {
        m_blob_slots[it->second].active = true;
      }
    }
    
    // Free inactive slots
    if (m_device)
    {
      auto& root = m_device->get_root_node();
      
      // Free inactive token slots
      if (auto* tok_node = root.find_child(std::string_view("tok")))
      {
        for (size_t i = 0; i < m_object_slots.size(); ++i)
        {
          if (!m_object_slots[i].active && m_object_slots[i].session_id != -1)
          {
            if (auto* slot_node = tok_node->find_child(std::to_string(i)))
            {
              if (auto* param = slot_node->find_child(std::string_view("active"))->get_parameter())
                param->set_value(false);
            }
            m_object_session_to_slot.erase(m_object_slots[i].session_id);
            m_object_slots[i].session_id = -1;
          }
        }
      }
      
      // Free inactive pointer slots
      if (auto* ptr_node = root.find_child(std::string_view("ptr")))
      {
        for (size_t i = 0; i < m_cursor_slots.size(); ++i)
        {
          if (!m_cursor_slots[i].active && m_cursor_slots[i].session_id != -1)
          {
            if (auto* slot_node = ptr_node->find_child(std::to_string(i)))
            {
              if (auto* param = slot_node->find_child(std::string_view("active"))->get_parameter())
                param->set_value(false);
            }
            m_cursor_session_to_slot.erase(m_cursor_slots[i].session_id);
            m_cursor_slots[i].session_id = -1;
          }
        }
      }
      
      // Free inactive bounds slots
      if (auto* bnd_node = root.find_child(std::string_view("bnd")))
      {
        for (size_t i = 0; i < m_blob_slots.size(); ++i)
        {
          if (!m_blob_slots[i].active && m_blob_slots[i].session_id != -1)
          {
            if (auto* slot_node = bnd_node->find_child(std::to_string(i)))
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
  catch (const std::exception& e)
  {
    // Invalid message
  }
}

void TUIOProtocol::update_tuio2_token(int slot, const TUIO2Token& tok)
{
  if (!m_device)
    return;
  
  auto& root = m_device->get_root_node();
  auto* tok_node = root.find_child(std::string_view("tok"));
  if (!tok_node)
    return;
  
  auto* slot_node = tok_node->find_child(std::to_string(slot));
  if (!slot_node)
    return;
  
  // Update all parameters
  if (auto* param = slot_node->find_child(std::string_view("session_id"))->get_parameter())
    param->set_value((int)tok.session_id);
  if (auto* param = slot_node->find_child(std::string_view("active"))->get_parameter())
    param->set_value(true);
  if (auto* param = slot_node->find_child(std::string_view("user_id"))->get_parameter())
    param->set_value((int)tok.user_id);
  if (auto* param = slot_node->find_child(std::string_view("type_id"))->get_parameter())
    param->set_value((int)tok.type_id);
  if (auto* param = slot_node->find_child(std::string_view("component_id"))->get_parameter())
    param->set_value((int)tok.component_id);
  if (auto* param = slot_node->find_child(std::string_view("position"))->get_parameter())
    param->set_value(ossia::vec2f{tok.x, tok.y});
  if (auto* param = slot_node->find_child(std::string_view("angle"))->get_parameter())
    param->set_value(tok.angle);
  
  if (tok.x_vel && tok.y_vel)
  {
    if (auto* param = slot_node->find_child(std::string_view("velocity"))->get_parameter())
      param->set_value(ossia::vec2f{*tok.x_vel, *tok.y_vel});
  }
  if (tok.a_vel)
  {
    if (auto* param = slot_node->find_child(std::string_view("angle_velocity"))->get_parameter())
      param->set_value(*tok.a_vel);
  }
  if (tok.m_acc)
  {
    if (auto* param = slot_node->find_child(std::string_view("motion_acceleration"))->get_parameter())
      param->set_value(*tok.m_acc);
  }
  if (tok.r_acc)
  {
    if (auto* param = slot_node->find_child(std::string_view("rotation_acceleration"))->get_parameter())
      param->set_value(*tok.r_acc);
  }
}

void TUIOProtocol::update_tuio2_pointer(int slot, const TUIO2Pointer& ptr)
{
  if (!m_device)
    return;
  
  auto& root = m_device->get_root_node();
  auto* ptr_node = root.find_child(std::string_view("ptr"));
  if (!ptr_node)
    return;
  
  auto* slot_node = ptr_node->find_child(std::to_string(slot));
  if (!slot_node)
    return;
  
  // Update all parameters
  if (auto* param = slot_node->find_child(std::string_view("session_id"))->get_parameter())
    param->set_value((int)ptr.session_id);
  if (auto* param = slot_node->find_child(std::string_view("active"))->get_parameter())
    param->set_value(true);
  if (auto* param = slot_node->find_child(std::string_view("user_id"))->get_parameter())
    param->set_value((int)ptr.user_id);
  if (auto* param = slot_node->find_child(std::string_view("type_id"))->get_parameter())
    param->set_value((int)ptr.type_id);
  if (auto* param = slot_node->find_child(std::string_view("component_id"))->get_parameter())
    param->set_value((int)ptr.component_id);
  if (auto* param = slot_node->find_child(std::string_view("position"))->get_parameter())
    param->set_value(ossia::vec2f{ptr.x, ptr.y});
  if (auto* param = slot_node->find_child(std::string_view("angle"))->get_parameter())
    param->set_value(ptr.angle);
  if (auto* param = slot_node->find_child(std::string_view("shear"))->get_parameter())
    param->set_value(ptr.shear);
  if (auto* param = slot_node->find_child(std::string_view("radius"))->get_parameter())
    param->set_value(ptr.radius);
  if (auto* param = slot_node->find_child(std::string_view("pressure"))->get_parameter())
    param->set_value(ptr.pressure);
  
  if (ptr.x_vel && ptr.y_vel)
  {
    if (auto* param = slot_node->find_child(std::string_view("velocity"))->get_parameter())
      param->set_value(ossia::vec2f{*ptr.x_vel, *ptr.y_vel});
  }
  if (ptr.p_vel)
  {
    if (auto* param = slot_node->find_child(std::string_view("pressure_velocity"))->get_parameter())
      param->set_value(*ptr.p_vel);
  }
  if (ptr.m_acc)
  {
    if (auto* param = slot_node->find_child(std::string_view("motion_acceleration"))->get_parameter())
      param->set_value(*ptr.m_acc);
  }
  if (ptr.p_acc)
  {
    if (auto* param = slot_node->find_child(std::string_view("pressure_acceleration"))->get_parameter())
      param->set_value(*ptr.p_acc);
  }
}

void TUIOProtocol::update_tuio2_bounds(int slot, const TUIO2Bounds& bnd)
{
  if (!m_device)
    return;
  
  auto& root = m_device->get_root_node();
  auto* bnd_node = root.find_child(std::string_view("bnd"));
  if (!bnd_node)
    return;
  
  auto* slot_node = bnd_node->find_child(std::to_string(slot));
  if (!slot_node)
    return;
  
  // Update all parameters
  if (auto* param = slot_node->find_child(std::string_view("session_id"))->get_parameter())
    param->set_value((int)bnd.session_id);
  if (auto* param = slot_node->find_child(std::string_view("active"))->get_parameter())
    param->set_value(true);
  if (auto* param = slot_node->find_child(std::string_view("position"))->get_parameter())
    param->set_value(ossia::vec2f{bnd.x, bnd.y});
  if (auto* param = slot_node->find_child(std::string_view("angle"))->get_parameter())
    param->set_value(bnd.angle);
  if (auto* param = slot_node->find_child(std::string_view("size"))->get_parameter())
    param->set_value(ossia::vec2f{bnd.width, bnd.height});
  if (auto* param = slot_node->find_child(std::string_view("area"))->get_parameter())
    param->set_value(bnd.area);
  
  if (bnd.x_vel && bnd.y_vel)
  {
    if (auto* param = slot_node->find_child(std::string_view("velocity"))->get_parameter())
      param->set_value(ossia::vec2f{*bnd.x_vel, *bnd.y_vel});
  }
  if (bnd.a_vel)
  {
    if (auto* param = slot_node->find_child(std::string_view("angle_velocity"))->get_parameter())
      param->set_value(*bnd.a_vel);
  }
  if (bnd.m_acc)
  {
    if (auto* param = slot_node->find_child(std::string_view("motion_acceleration"))->get_parameter())
      param->set_value(*bnd.m_acc);
  }
  if (bnd.r_acc)
  {
    if (auto* param = slot_node->find_child(std::string_view("rotation_acceleration"))->get_parameter())
      param->set_value(*bnd.r_acc);
  }
}

}