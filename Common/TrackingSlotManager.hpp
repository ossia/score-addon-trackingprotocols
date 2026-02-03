#pragma once

#include "TrackingTypes.hpp"
#include <ossia/detail/hash_map.hpp>
#include <vector>
#include <string>
#include <algorithm>

namespace Tracking
{

// SlotInfo stores mapping information for a slot
struct SlotInfo
{
  int32_t id{-1};
  std::string name;
  bool active{false};
};

// Template-based slot manager
template<typename EntityT>
class SlotManager
{
public:
  explicit SlotManager(int num_slots, SlotStrategy strategy = SlotStrategy::SessionBased)
      : m_strategy{strategy}
  {
    m_slots.resize(num_slots);
    m_entities.resize(num_slots);
  }

  // Get the number of slots
  int size() const { return static_cast<int>(m_slots.size()); }

  // Get slot info
  const SlotInfo& slot_info(int slot) const { return m_slots[slot]; }
  SlotInfo& slot_info(int slot) { return m_slots[slot]; }

  // Get entity at slot
  const EntityT& entity(int slot) const { return m_entities[slot]; }
  EntityT& entity(int slot) { return m_entities[slot]; }

  // Find or allocate slot for an entity by ID
  int find_or_allocate(int32_t id)
  {
    // Check if ID already has a slot
    auto it = m_id_to_slot.find(id);
    if (it != m_id_to_slot.end())
    {
      return it->second;
    }

    // Find first free slot
    for (int i = 0; i < static_cast<int>(m_slots.size()); ++i)
    {
      if (m_slots[i].id == -1)
      {
        m_slots[i].id = id;
        m_slots[i].active = true;
        m_id_to_slot[id] = i;
        return i;
      }
    }

    // No free slot - reuse oldest inactive slot
    for (int i = 0; i < static_cast<int>(m_slots.size()); ++i)
    {
      if (!m_slots[i].active)
      {
        // Remove old mapping
        if (m_slots[i].id != -1)
        {
          m_id_to_slot.erase(m_slots[i].id);
          m_name_to_slot.erase(m_slots[i].name);
        }

        m_slots[i].id = id;
        m_slots[i].name.clear();
        m_slots[i].active = true;
        m_id_to_slot[id] = i;
        return i;
      }
    }

    // All slots are active - use slot 0 as fallback
    return 0;
  }

  // Find or allocate slot for an entity by name
  int find_or_allocate(const std::string& name)
  {
    // Check if name already has a slot
    auto it = m_name_to_slot.find(name);
    if (it != m_name_to_slot.end())
    {
      return it->second;
    }

    // Find first free slot
    for (int i = 0; i < static_cast<int>(m_slots.size()); ++i)
    {
      if (m_slots[i].id == -1 && m_slots[i].name.empty())
      {
        m_slots[i].name = name;
        m_slots[i].active = true;
        m_name_to_slot[name] = i;
        return i;
      }
    }

    // No free slot - reuse oldest inactive slot
    for (int i = 0; i < static_cast<int>(m_slots.size()); ++i)
    {
      if (!m_slots[i].active)
      {
        // Remove old mapping
        if (m_slots[i].id != -1)
        {
          m_id_to_slot.erase(m_slots[i].id);
        }
        m_name_to_slot.erase(m_slots[i].name);

        m_slots[i].id = -1;
        m_slots[i].name = name;
        m_slots[i].active = true;
        m_name_to_slot[name] = i;
        return i;
      }
    }

    return 0;
  }

  // Find or allocate slot for an entity by ID and name
  int find_or_allocate(int32_t id, const std::string& name)
  {
    switch (m_strategy)
    {
      case SlotStrategy::SessionBased:
      case SlotStrategy::PersistentID:
        return find_or_allocate(id);
      case SlotStrategy::NameBased:
        return find_or_allocate(name);
    }
    return find_or_allocate(id);
  }

  // Find slot by ID
  int find_by_id(int32_t id) const
  {
    auto it = m_id_to_slot.find(id);
    if (it != m_id_to_slot.end())
    {
      return it->second;
    }
    return -1;
  }

  // Find slot by name
  int find_by_name(const std::string& name) const
  {
    auto it = m_name_to_slot.find(name);
    if (it != m_name_to_slot.end())
    {
      return it->second;
    }
    return -1;
  }

  // Mark all slots as inactive (for ALIVE message processing)
  void mark_all_inactive()
  {
    for (auto& slot : m_slots)
    {
      slot.active = false;
    }
  }

  // Mark a slot as active
  void mark_active(int slot)
  {
    if (slot >= 0 && slot < static_cast<int>(m_slots.size()))
    {
      m_slots[slot].active = true;
    }
  }

  // Mark slot as active by ID
  void mark_active_by_id(int32_t id)
  {
    int slot = find_by_id(id);
    if (slot >= 0)
    {
      m_slots[slot].active = true;
    }
  }

  // Free slot by ID
  void free_slot(int32_t id)
  {
    auto it = m_id_to_slot.find(id);
    if (it != m_id_to_slot.end())
    {
      int slot = it->second;
      m_name_to_slot.erase(m_slots[slot].name);
      m_slots[slot].id = -1;
      m_slots[slot].name.clear();
      m_slots[slot].active = false;
      m_id_to_slot.erase(it);
    }
  }

  // Free inactive slots and return list of freed slot indices
  std::vector<int> free_inactive_slots()
  {
    std::vector<int> freed;
    for (int i = 0; i < static_cast<int>(m_slots.size()); ++i)
    {
      if (!m_slots[i].active && (m_slots[i].id != -1 || !m_slots[i].name.empty()))
      {
        if (m_slots[i].id != -1)
        {
          m_id_to_slot.erase(m_slots[i].id);
        }
        m_name_to_slot.erase(m_slots[i].name);

        m_slots[i].id = -1;
        m_slots[i].name.clear();
        freed.push_back(i);
      }
    }
    return freed;
  }

  // Get the strategy
  SlotStrategy strategy() const { return m_strategy; }

private:
  SlotStrategy m_strategy;
  std::vector<SlotInfo> m_slots;
  std::vector<EntityT> m_entities;
  ossia::hash_map<int32_t, int> m_id_to_slot;
  ossia::hash_map<std::string, int> m_name_to_slot;
};

}
