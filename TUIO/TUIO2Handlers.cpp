#include "TUIOProtocol.hpp"
#include "TUIO2Types.hpp"
#include "../Common/TrackingTreeBuilder.hpp"

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>

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
      using TB = Tracking::TreeBuilder;
      auto& root = m_device->get_root_node();
      TB::update_param(&root, "source", m_tuio2_source);
      TB::update_param(&root, "frame", (int)m_tuio2_frame_id);
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
    int slot = m_object_slots.find_or_allocate(session_id);

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
    m_object_slots.mark_active(slot);

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
    int slot = m_cursor_slots.find_or_allocate(session_id);

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
    m_cursor_slots.mark_active(slot);

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
    int slot = m_blob_slots.find_or_allocate(session_id);

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
    m_blob_slots.mark_active(slot);

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
    int slot = m_object_slots.find_by_id(session_id);
    if (slot >= 0)
    {
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
            using TB = Tracking::TreeBuilder;
            TB::update_param(slot_node, "symbol_group", group);
            TB::update_param(slot_node, "symbol_data", data);
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

    // Find the slot for this session
    int slot = m_object_slots.find_by_id(session_id);
    if (slot >= 0)
    {
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
    std::vector<int32_t> alive_ids;

    auto args = msg.ArgumentsBegin();
    while (args != msg.ArgumentsEnd())
    {
      alive_ids.push_back((args++)->AsInt32());
    }

    if (!m_device)
      return;

    auto& root = m_device->get_root_node();

    // Process tokens
    m_object_slots.mark_all_inactive();
    for (int32_t id : alive_ids)
      m_object_slots.mark_active_by_id(id);

    if (auto* tok_node = root.find_child(std::string_view("tok")))
    {
      auto freed = m_object_slots.free_inactive_slots();
      for (int slot_idx : freed)
      {
        if (auto* slot_node = tok_node->find_child(std::to_string(slot_idx)))
        {
          Tracking::TreeBuilder::update_param(slot_node, "active", false);
        }
      }
    }

    // Process pointers
    m_cursor_slots.mark_all_inactive();
    for (int32_t id : alive_ids)
      m_cursor_slots.mark_active_by_id(id);

    if (auto* ptr_node = root.find_child(std::string_view("ptr")))
    {
      auto freed = m_cursor_slots.free_inactive_slots();
      for (int slot_idx : freed)
      {
        if (auto* slot_node = ptr_node->find_child(std::to_string(slot_idx)))
        {
          Tracking::TreeBuilder::update_param(slot_node, "active", false);
        }
      }
    }

    // Process bounds
    m_blob_slots.mark_all_inactive();
    for (int32_t id : alive_ids)
      m_blob_slots.mark_active_by_id(id);

    if (auto* bnd_node = root.find_child(std::string_view("bnd")))
    {
      auto freed = m_blob_slots.free_inactive_slots();
      for (int slot_idx : freed)
      {
        if (auto* slot_node = bnd_node->find_child(std::to_string(slot_idx)))
        {
          Tracking::TreeBuilder::update_param(slot_node, "active", false);
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

  using TB = Tracking::TreeBuilder;
  TB::update_param(slot_node, "session_id", (int)tok.session_id);
  TB::update_param(slot_node, "active", true);
  TB::update_param(slot_node, "user_id", (int)tok.user_id);
  TB::update_param(slot_node, "type_id", (int)tok.type_id);
  TB::update_param(slot_node, "component_id", (int)tok.component_id);
  TB::update_param(slot_node, "position", ossia::vec2f{tok.x, tok.y});
  TB::update_param(slot_node, "angle", tok.angle);

  if (tok.x_vel && tok.y_vel)
  {
    TB::update_param(slot_node, "velocity", ossia::vec2f{*tok.x_vel, *tok.y_vel});
  }
  if (tok.a_vel)
  {
    TB::update_param(slot_node, "angle_velocity", *tok.a_vel);
  }
  if (tok.m_acc)
  {
    TB::update_param(slot_node, "motion_acceleration", *tok.m_acc);
  }
  if (tok.r_acc)
  {
    TB::update_param(slot_node, "rotation_acceleration", *tok.r_acc);
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

  using TB = Tracking::TreeBuilder;
  TB::update_param(slot_node, "session_id", (int)ptr.session_id);
  TB::update_param(slot_node, "active", true);
  TB::update_param(slot_node, "user_id", (int)ptr.user_id);
  TB::update_param(slot_node, "type_id", (int)ptr.type_id);
  TB::update_param(slot_node, "component_id", (int)ptr.component_id);
  TB::update_param(slot_node, "position", ossia::vec2f{ptr.x, ptr.y});
  TB::update_param(slot_node, "angle", ptr.angle);
  TB::update_param(slot_node, "shear", ptr.shear);
  TB::update_param(slot_node, "radius", ptr.radius);
  TB::update_param(slot_node, "pressure", ptr.pressure);

  if (ptr.x_vel && ptr.y_vel)
  {
    TB::update_param(slot_node, "velocity", ossia::vec2f{*ptr.x_vel, *ptr.y_vel});
  }
  if (ptr.p_vel)
  {
    TB::update_param(slot_node, "pressure_velocity", *ptr.p_vel);
  }
  if (ptr.m_acc)
  {
    TB::update_param(slot_node, "motion_acceleration", *ptr.m_acc);
  }
  if (ptr.p_acc)
  {
    TB::update_param(slot_node, "pressure_acceleration", *ptr.p_acc);
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

  using TB = Tracking::TreeBuilder;
  TB::update_param(slot_node, "session_id", (int)bnd.session_id);
  TB::update_param(slot_node, "active", true);
  TB::update_param(slot_node, "position", ossia::vec2f{bnd.x, bnd.y});
  TB::update_param(slot_node, "angle", bnd.angle);
  TB::update_param(slot_node, "size", ossia::vec2f{bnd.width, bnd.height});
  TB::update_param(slot_node, "area", bnd.area);

  if (bnd.x_vel && bnd.y_vel)
  {
    TB::update_param(slot_node, "velocity", ossia::vec2f{*bnd.x_vel, *bnd.y_vel});
  }
  if (bnd.a_vel)
  {
    TB::update_param(slot_node, "angle_velocity", *bnd.a_vel);
  }
  if (bnd.m_acc)
  {
    TB::update_param(slot_node, "motion_acceleration", *bnd.m_acc);
  }
  if (bnd.r_acc)
  {
    TB::update_param(slot_node, "rotation_acceleration", *bnd.r_acc);
  }
}

}
