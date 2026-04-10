#pragma once

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <memory>
#include <slirp/libslirp.h>
#include <vector>

namespace aarchvm {

class SlirpNetBackend final {
public:
  using RxFrameCallback = std::function<void(std::vector<std::uint8_t>)>;

  explicit SlirpNetBackend(RxFrameCallback rx_frame_callback);
  ~SlirpNetBackend();

  SlirpNetBackend(const SlirpNetBackend&) = delete;
  SlirpNetBackend& operator=(const SlirpNetBackend&) = delete;

  [[nodiscard]] bool ready() const { return slirp_ != nullptr; }
  void send_guest_frame(std::vector<std::uint8_t> frame);
  [[nodiscard]] bool poll(std::uint32_t max_wait_ms = 0);

  [[nodiscard]] bool save_state(std::ostream& out) const;
  [[nodiscard]] bool load_state(std::istream& in);

private:
  struct Timer;

  [[nodiscard]] std::int64_t clock_now_ns() const;
  [[nodiscard]] std::int64_t clock_now_ms() const;
  void process_expired_timers();
  void erase_timer(Timer* timer);
  [[nodiscard]] bool consume_activity_flag();

  static Timer* timer_from_handle(void* handle) { return static_cast<Timer*>(handle); }

  static slirp_ssize_t send_packet_cb(const void* buf, std::size_t len, void* opaque);
  static void guest_error_cb(const char* msg, void* opaque);
  static std::int64_t clock_get_ns_cb(void* opaque);
#if SLIRP_CHECK_VERSION(4, 7, 0)
  static void* timer_new_opaque_cb(SlirpTimerId id, void* cb_opaque, void* opaque);
#else
  static void* timer_new_cb(SlirpTimerCb cb, void* cb_opaque, void* opaque);
#endif
  static void timer_free_cb(void* timer, void* opaque);
  static void timer_mod_cb(void* timer, std::int64_t expire_time, void* opaque);
#if SLIRP_CHECK_VERSION(4, 9, 0)
  static void register_poll_socket_cb(slirp_os_socket fd, void* opaque);
  static void unregister_poll_socket_cb(slirp_os_socket fd, void* opaque);
#else
  static void register_poll_socket_cb(int fd, void* opaque);
  static void unregister_poll_socket_cb(int fd, void* opaque);
#endif
  static void notify_cb(void* opaque);
  static int add_poll_fd_cb(
#if SLIRP_CHECK_VERSION(4, 9, 0)
      slirp_os_socket
#else
      int
#endif
          fd,
      int events,
      void* opaque);
  static int get_revents_cb(int index, void* opaque);
  static slirp_ssize_t save_state_cb(const void* buf, std::size_t len, void* opaque);
  static slirp_ssize_t load_state_cb(void* buf, std::size_t len, void* opaque);

  RxFrameCallback rx_frame_callback_;
  Slirp* slirp_ = nullptr;
  SlirpCb callbacks_{};
  std::vector<Timer*> timers_;
  bool activity_pending_ = false;
};

} // namespace aarchvm
