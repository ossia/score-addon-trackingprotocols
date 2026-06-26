#pragma once

#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/domain/domain.hpp>

#include <string>
#include <cmath>

namespace Tracking
{

// Static utilities for creating device tree nodes
struct TreeBuilder
{
  // Create a VEC2F parameter
  static ossia::net::parameter_base* create_vec2f_param(
      ossia::net::node_base& parent,
      const std::string& name,
      ossia::vec2f default_val = {0.f, 0.f},
      float domain_min = 0.f,
      float domain_max = 1.f)
  {
    auto* node = parent.create_child(name);
    auto* param = node->create_parameter(ossia::val_type::VEC2F);
    param->set_domain(ossia::make_domain(domain_min, domain_max));
    param->set_value(default_val);
    return param;
  }

  // Create a VEC3F parameter
  static ossia::net::parameter_base* create_vec3f_param(
      ossia::net::node_base& parent,
      const std::string& name,
      ossia::vec3f default_val = {0.f, 0.f, 0.f},
      float domain_min = -10000.f,
      float domain_max = 10000.f)
  {
    auto* node = parent.create_child(name);
    auto* param = node->create_parameter(ossia::val_type::VEC3F);
    param->set_domain(ossia::make_domain(domain_min, domain_max));
    param->set_value(default_val);
    return param;
  }

  // Create a VEC4F parameter (for quaternions)
  static ossia::net::parameter_base* create_vec4f_param(
      ossia::net::node_base& parent,
      const std::string& name,
      ossia::vec4f default_val = {0.f, 0.f, 0.f, 1.f})
  {
    auto* node = parent.create_child(name);
    auto* param = node->create_parameter(ossia::val_type::VEC4F);
    param->set_domain(ossia::make_domain(-1.f, 1.f));
    param->set_value(default_val);
    return param;
  }

  // Create a FLOAT parameter
  static ossia::net::parameter_base* create_float_param(
      ossia::net::node_base& parent,
      const std::string& name,
      float default_val = 0.f,
      float domain_min = 0.f,
      float domain_max = 1.f)
  {
    auto* node = parent.create_child(name);
    auto* param = node->create_parameter(ossia::val_type::FLOAT);
    param->set_domain(ossia::make_domain(domain_min, domain_max));
    param->set_value(default_val);
    return param;
  }

  // Create an INT parameter
  static ossia::net::parameter_base* create_int_param(
      ossia::net::node_base& parent,
      const std::string& name,
      int32_t default_val = 0)
  {
    auto* node = parent.create_child(name);
    auto* param = node->create_parameter(ossia::val_type::INT);
    param->set_value(default_val);
    return param;
  }

  // Create a BOOL parameter
  static ossia::net::parameter_base* create_bool_param(
      ossia::net::node_base& parent,
      const std::string& name,
      bool default_val = false)
  {
    auto* node = parent.create_child(name);
    auto* param = node->create_parameter(ossia::val_type::BOOL);
    param->set_value(default_val);
    return param;
  }

  // Create a LIST parameter (dynamic-size array of ossia::value).
  // Use update_param(node, name, std::vector<ossia::value>{...}) to update.
  static ossia::net::parameter_base* create_list_param(
      ossia::net::node_base& parent,
      const std::string& name)
  {
    auto* node = parent.create_child(name);
    auto* param = node->create_parameter(ossia::val_type::LIST);
    param->set_value(std::vector<ossia::value>{});
    return param;
  }

  // Create a STRING parameter
  static ossia::net::parameter_base* create_string_param(
      ossia::net::node_base& parent,
      const std::string& name,
      const std::string& default_val = "")
  {
    auto* node = parent.create_child(name);
    auto* param = node->create_parameter(ossia::val_type::STRING);
    param->set_value(default_val);
    return param;
  }

  // Create a tracked point slot (for cursors, markers, LEDs)
  static ossia::net::node_base* create_point_slot(
      ossia::net::node_base& parent,
      int slot_index,
      bool include_velocity = true,
      bool include_acceleration = true,
      bool include_3d = false)
  {
    auto* slot_node = parent.create_child(std::to_string(slot_index));

    create_int_param(*slot_node, "session_id", -1);
    create_bool_param(*slot_node, "active", false);

    if (include_3d)
    {
      create_vec3f_param(*slot_node, "position");
      if (include_velocity)
        create_vec3f_param(*slot_node, "velocity");
      if (include_acceleration)
        create_vec3f_param(*slot_node, "acceleration");
    }
    else
    {
      create_vec2f_param(*slot_node, "position", {0.5f, 0.5f});
      if (include_velocity)
        create_vec2f_param(*slot_node, "velocity", {0.f, 0.f}, -10.f, 10.f);
      if (include_acceleration)
        create_float_param(*slot_node, "motion_acceleration", 0.f, 0.f, 100.f);
    }

    return slot_node;
  }

  // Create a tracked object slot (rigid body with orientation)
  static ossia::net::node_base* create_object_slot(
      ossia::net::node_base& parent,
      int slot_index,
      bool include_velocity = true,
      bool include_acceleration = true,
      bool include_euler = true,
      bool include_quaternion = false,
      bool include_3d = false)
  {
    auto* slot_node = parent.create_child(std::to_string(slot_index));

    create_int_param(*slot_node, "session_id", -1);
    create_bool_param(*slot_node, "active", false);

    if (include_3d)
    {
      create_vec3f_param(*slot_node, "position");
      if (include_euler)
        create_vec3f_param(*slot_node, "orientation", {0.f, 0.f, 0.f}, static_cast<float>(-M_PI), static_cast<float>(M_PI));
      if (include_quaternion)
        create_vec4f_param(*slot_node, "quaternion");
      if (include_velocity)
        create_vec3f_param(*slot_node, "velocity");
      if (include_acceleration)
        create_vec3f_param(*slot_node, "acceleration");
    }
    else
    {
      create_vec2f_param(*slot_node, "position", {0.5f, 0.5f});
      if (include_euler)
        create_float_param(*slot_node, "angle", 0.f, 0.f, static_cast<float>(2 * M_PI));
      if (include_velocity)
      {
        create_vec2f_param(*slot_node, "velocity", {0.f, 0.f}, -10.f, 10.f);
        create_float_param(*slot_node, "angle_velocity", 0.f, static_cast<float>(-2 * M_PI), static_cast<float>(2 * M_PI));
      }
      if (include_acceleration)
      {
        create_float_param(*slot_node, "motion_acceleration", 0.f, 0.f, 100.f);
        create_float_param(*slot_node, "rotation_acceleration", 0.f, 0.f, 100.f);
      }
    }

    return slot_node;
  }

  // Create a tracked bounds slot (blobs)
  static ossia::net::node_base* create_bounds_slot(
      ossia::net::node_base& parent,
      int slot_index,
      bool include_velocity = true,
      bool include_acceleration = true,
      bool include_3d = false)
  {
    auto* slot_node = parent.create_child(std::to_string(slot_index));

    create_int_param(*slot_node, "session_id", -1);
    create_bool_param(*slot_node, "active", false);

    if (include_3d)
    {
      create_vec3f_param(*slot_node, "position");
      create_vec3f_param(*slot_node, "orientation", {0.f, 0.f, 0.f}, static_cast<float>(-M_PI), static_cast<float>(M_PI));
      create_vec3f_param(*slot_node, "size");
      create_float_param(*slot_node, "volume", 0.f, 0.f, 1.f);
      if (include_velocity)
        create_vec3f_param(*slot_node, "velocity");
      if (include_acceleration)
        create_vec3f_param(*slot_node, "acceleration");
    }
    else
    {
      create_vec2f_param(*slot_node, "position", {0.5f, 0.5f});
      create_float_param(*slot_node, "angle", 0.f, 0.f, static_cast<float>(2 * M_PI));
      create_vec2f_param(*slot_node, "size", {0.1f, 0.1f});
      create_float_param(*slot_node, "area", 0.01f, 0.f, 1.f);
      if (include_velocity)
      {
        create_vec2f_param(*slot_node, "velocity", {0.f, 0.f}, -10.f, 10.f);
        create_float_param(*slot_node, "angle_velocity", 0.f, static_cast<float>(-2 * M_PI), static_cast<float>(2 * M_PI));
      }
      if (include_acceleration)
      {
        create_float_param(*slot_node, "motion_acceleration", 0.f, 0.f, 100.f);
        create_float_param(*slot_node, "rotation_acceleration", 0.f, 0.f, 100.f);
      }
    }

    return slot_node;
  }

  // Create LED slots under a parent
  static void create_led_slots(
      ossia::net::node_base& parent,
      int num_leds)
  {
    auto* leds_node = parent.create_child("leds");
    for (int i = 0; i < num_leds; ++i)
    {
      auto* led_node = leds_node->create_child(std::to_string(i));

      create_bool_param(*led_node, "active", false);
      create_int_param(*led_node, "index", i);
      create_vec3f_param(*led_node, "position");
      create_vec3f_param(*led_node, "velocity");
      create_vec3f_param(*led_node, "acceleration");
    }
  }

  // Create zone slot
  static ossia::net::node_base* create_zone_slot(
      ossia::net::node_base& parent,
      const std::string& zone_name)
  {
    auto* zone_node = parent.create_child(zone_name);
    create_bool_param(*zone_node, "occupied", false);
    return zone_node;
  }

  // Helper to update a parameter value safely
  static void update_param(ossia::net::node_base* node, const std::string_view& name, const ossia::value& val)
  {
    if (!node)
      return;
    if (auto* child = node->find_child(name))
    {
      if (auto* param = child->get_parameter())
      {
        param->set_value(val);
      }
    }
  }

  // Helper to get a node's parameter
  static ossia::net::parameter_base* get_param(ossia::net::node_base* node, const std::string_view& name)
  {
    if (!node)
      return nullptr;
    if (auto* child = node->find_child(name))
    {
      return child->get_parameter();
    }
    return nullptr;
  }
};

}
