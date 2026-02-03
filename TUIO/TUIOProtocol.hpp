#pragma once

#include "TUIO2Types.hpp"
#include "TUIOSpecificSettings.hpp"
#include "../Common/TrackingTypes.hpp"
#include "../Common/TrackingSlotManager.hpp"

#include <ossia/detail/logger.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/base/listening.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/osc/detail/osc_packet_processor.hpp>
#include <ossia/network/sockets/udp_socket.hpp>

#include <chrono>

namespace TUIO
{

// TUIO-specific entity types that extend the common types
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
      int numBlobs = 8,
      TUIOVersion version = TUIOVersion::V1_1);

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

  // TUIO 1.1 handlers
  void handle_2Dobj_message(const oscpack::ReceivedMessage& msg);
  void handle_2Dcur_message(const oscpack::ReceivedMessage& msg);
  void handle_2Dblb_message(const oscpack::ReceivedMessage& msg);

  // TUIO 2.0 handlers
  void handle_tuio2_frm(const oscpack::ReceivedMessage& msg);
  void handle_tuio2_tok(const oscpack::ReceivedMessage& msg);
  void handle_tuio2_ptr(const oscpack::ReceivedMessage& msg);
  void handle_tuio2_bnd(const oscpack::ReceivedMessage& msg);
  void handle_tuio2_sym(const oscpack::ReceivedMessage& msg);
  void handle_tuio2_alv(const oscpack::ReceivedMessage& msg);
  void handle_tuio2_ctl(const oscpack::ReceivedMessage& msg);

  // TUIO 1.1 processors
  void process_2Dobj_set(const oscpack::ReceivedMessage& msg);
  void process_2Dobj_alive(const oscpack::ReceivedMessage& msg);
  void process_2Dcur_set(const oscpack::ReceivedMessage& msg);
  void process_2Dcur_alive(const oscpack::ReceivedMessage& msg);
  void process_2Dblb_set(const oscpack::ReceivedMessage& msg);
  void process_2Dblb_alive(const oscpack::ReceivedMessage& msg);

  // TUIO 2.0 update methods
  void update_tuio2_token(int slot, const TUIO2Token& tok);
  void update_tuio2_pointer(int slot, const TUIO2Pointer& ptr);
  void update_tuio2_bounds(int slot, const TUIO2Bounds& bnd);

  void update_object_parameters(int slot, const TUIOObject& obj);
  void update_cursor_parameters(int slot, const TUIOCursor& cur);
  void update_blob_parameters(int slot, const TUIOBlob& blob);

  template<typename SlotMgr>
  void process_alive_message(const std::vector<int32_t>& alive_ids,
                             SlotMgr& slots, const std::string& node_name);

  ossia::net::network_context_ptr m_ctx;
  std::unique_ptr<ossia::net::udp_receive_socket> m_receive_socket;
  ossia::net::device_base* m_device{nullptr};

  // Slot managers using common infrastructure
  Tracking::SlotManager<TUIOObject> m_object_slots;
  Tracking::SlotManager<TUIOCursor> m_cursor_slots;
  Tracking::SlotManager<TUIOBlob> m_blob_slots;

  // TUIO 2.0 data per slot
  std::vector<TUIO2Token> m_tuio2_tokens;
  std::vector<TUIO2Pointer> m_tuio2_pointers;
  std::vector<TUIO2Bounds> m_tuio2_bounds;
  std::vector<TUIO2Symbol> m_tuio2_symbols;
  std::vector<TUIO2Control> m_tuio2_controls;

  int32_t m_frame_id{-1};
  std::string m_source;

  uint16_t m_port;
  int m_num_objects;
  int m_num_cursors;
  int m_num_blobs;
  TUIOVersion m_version;

  // TUIO 2.0 frame info
  uint32_t m_tuio2_frame_id{0};
  std::chrono::steady_clock::time_point m_tuio2_frame_time;
  uint16_t m_tuio2_dim_width{640};
  uint16_t m_tuio2_dim_height{480};
  std::string m_tuio2_source;

  // Tree creation methods
  void create_tuio1_tree(ossia::net::node_base& root);
  void create_tuio2_tree(ossia::net::node_base& root);
};

}
