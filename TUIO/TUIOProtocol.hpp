#pragma once

#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/base/listening.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/osc/detail/osc_packet_processor.hpp>
#include <ossia/network/sockets/udp_socket.hpp>

#include <optional>
#include <chrono>

namespace TUIO
{

struct TUIOObject
{
  int32_t session_id{-1};
  int32_t class_id{-1};
  float x{}, y{}, z{};
  float angle{}, angle_b{}, angle_c{};
  float x_vel{}, y_vel{}, z_vel{};
  float angle_vel{}, angle_vel_b{}, angle_vel_c{};
  float motion_accel{};
  float rotation_accel{};
  std::chrono::steady_clock::time_point last_update;
};

struct TUIOCursor
{
  int32_t session_id{-1};
  float x{}, y{}, z{};
  float x_vel{}, y_vel{}, z_vel{};
  float motion_accel{};
  std::chrono::steady_clock::time_point last_update;
};

struct TUIOBlob
{
  int32_t session_id{-1};
  float x{}, y{}, z{};
  float angle{}, angle_b{}, angle_c{};
  float width{}, height{}, depth{};
  float area{}, volume{};
  float x_vel{}, y_vel{}, z_vel{};
  float angle_vel{}, angle_vel_b{}, angle_vel_c{};
  float motion_accel{};
  float rotation_accel{};
  std::chrono::steady_clock::time_point last_update;
};

class TUIOProtocol final : public ossia::net::protocol_base
{
public:
  explicit TUIOProtocol(
      const ossia::net::network_context_ptr& ctx,
      uint16_t port = 3333,
      int numObjects = 8,
      int numCursors = 8,
      int numBlobs = 8);
  
  ~TUIOProtocol();

  void set_device(ossia::net::device_base& dev) override;
  
  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value&) override { return false; }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base& node_base) override { return false; }

private:
  void setup_receive_socket();
  void stop_receive();
  void on_received_message(const oscpack::ReceivedMessage& msg);
  
  void handle_2Dobj_message(const oscpack::ReceivedMessage& msg);
  void handle_2Dcur_message(const oscpack::ReceivedMessage& msg);
  void handle_2Dblb_message(const oscpack::ReceivedMessage& msg);
  
  void process_2Dobj_set(const oscpack::ReceivedMessage& msg);
  void process_2Dobj_alive(const oscpack::ReceivedMessage& msg);
  void process_2Dcur_set(const oscpack::ReceivedMessage& msg);
  void process_2Dcur_alive(const oscpack::ReceivedMessage& msg);
  void process_2Dblb_set(const oscpack::ReceivedMessage& msg);
  void process_2Dblb_alive(const oscpack::ReceivedMessage& msg);
  
  void update_object_parameters(int32_t session_id, const TUIOObject& obj);
  void update_cursor_parameters(int32_t session_id, const TUIOCursor& cur);
  void update_blob_parameters(int32_t session_id, const TUIOBlob& blob);
  
  void remove_dead_sessions(const std::vector<int32_t>& alive_ids, const std::string& profile);
  
  ossia::net::network_context_ptr m_ctx;
  std::unique_ptr<ossia::net::udp_receive_socket> m_receive_socket;
  ossia::net::device_base* m_device{nullptr};
  
  // Fixed slots for each profile type
  struct SlotMapping {
    int32_t session_id{-1};  // -1 means slot is free
    bool active{false};
  };
  
  std::vector<SlotMapping> m_object_slots;
  std::vector<SlotMapping> m_cursor_slots;
  std::vector<SlotMapping> m_blob_slots;
  
  // Map from session_id to slot index
  ossia::hash_map<int32_t, int> m_object_session_to_slot;
  ossia::hash_map<int32_t, int> m_cursor_session_to_slot;
  ossia::hash_map<int32_t, int> m_blob_session_to_slot;
  
  // Store actual data per slot
  std::vector<TUIOObject> m_objects;
  std::vector<TUIOCursor> m_cursors;
  std::vector<TUIOBlob> m_blobs;
  
  int32_t m_frame_id{-1};
  std::string m_source;
  
  uint16_t m_port;
  int m_num_objects;
  int m_num_cursors;
  int m_num_blobs;
  
  // Helper methods for slot management
  int find_or_allocate_slot(int32_t session_id, std::vector<SlotMapping>& slots, 
                            ossia::hash_map<int32_t, int>& session_map);
  void free_slot(int32_t session_id, std::vector<SlotMapping>& slots,
                 ossia::hash_map<int32_t, int>& session_map);
};

}