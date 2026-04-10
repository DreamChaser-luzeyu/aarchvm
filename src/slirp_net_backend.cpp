#include "aarchvm/slirp_net_backend.hpp"

#include "aarchvm/snapshot_io.hpp"

#include <arpa/inet.h>
#include <poll.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace aarchvm {

namespace {

struct StateLoadCursor {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0;
  std::size_t offset = 0;
};

in_addr make_ipv4_addr(const char* text) {
  in_addr addr{};
  if (::inet_pton(AF_INET, text, &addr) != 1) {
    std::cerr << "Invalid IPv4 literal for slirp backend: " << text << '\n';
  }
  return addr;
}

short poll_events_from_slirp(int events) {
  short out = 0;
  if ((events & SLIRP_POLL_IN) != 0) {
    out |= POLLIN;
  }
  if ((events & SLIRP_POLL_OUT) != 0) {
    out |= POLLOUT;
  }
  if ((events & SLIRP_POLL_PRI) != 0) {
    out |= POLLPRI;
  }
  if ((events & SLIRP_POLL_ERR) != 0) {
    out |= POLLERR;
  }
  if ((events & SLIRP_POLL_HUP) != 0) {
    out |= POLLHUP;
  }
  return out;
}

int slirp_events_from_poll(short events) {
  int out = 0;
  if ((events & POLLIN) != 0) {
    out |= SLIRP_POLL_IN;
  }
  if ((events & POLLOUT) != 0) {
    out |= SLIRP_POLL_OUT;
  }
  if ((events & POLLPRI) != 0) {
    out |= SLIRP_POLL_PRI;
  }
  if ((events & POLLERR) != 0) {
    out |= SLIRP_POLL_ERR;
  }
  if ((events & POLLHUP) != 0) {
    out |= SLIRP_POLL_HUP;
  }
  return out;
}

} // namespace

struct SlirpNetBackend::Timer {
#if SLIRP_CHECK_VERSION(4, 7, 0)
  SlirpTimerId id = SLIRP_TIMER_RA;
#else
  SlirpTimerCb cb = nullptr;
#endif
  void* cb_opaque = nullptr;
  bool armed = false;
  std::int64_t expire_time_ms = 0;
};

SlirpNetBackend::SlirpNetBackend(RxFrameCallback rx_frame_callback)
    : rx_frame_callback_(std::move(rx_frame_callback)) {
  SlirpConfig cfg{};
  cfg.version = SLIRP_CONFIG_VERSION_MAX;
  cfg.restricted = 0;
  cfg.in_enabled = true;
  cfg.vnetwork = make_ipv4_addr("10.0.2.0");
  cfg.vnetmask = make_ipv4_addr("255.255.255.0");
  cfg.vhost = make_ipv4_addr("10.0.2.2");
  cfg.in6_enabled = false;
  cfg.vhostname = "aarchvm";
  cfg.vdhcp_start = make_ipv4_addr("10.0.2.15");
  cfg.vnameserver = make_ipv4_addr("10.0.2.3");
  cfg.if_mtu = 1500;
  cfg.if_mru = 1500;
  cfg.disable_host_loopback = false;
  cfg.enable_emu = false;
  cfg.disable_dns = false;
  cfg.disable_dhcp = false;

  callbacks_.send_packet = &SlirpNetBackend::send_packet_cb;
  callbacks_.guest_error = &SlirpNetBackend::guest_error_cb;
  callbacks_.clock_get_ns = &SlirpNetBackend::clock_get_ns_cb;
#if SLIRP_CHECK_VERSION(4, 7, 0)
  callbacks_.timer_new_opaque = &SlirpNetBackend::timer_new_opaque_cb;
#else
  callbacks_.timer_new = &SlirpNetBackend::timer_new_cb;
#endif
  callbacks_.timer_free = &SlirpNetBackend::timer_free_cb;
  callbacks_.timer_mod = &SlirpNetBackend::timer_mod_cb;
#if SLIRP_CHECK_VERSION(4, 9, 0)
  callbacks_.register_poll_socket = &SlirpNetBackend::register_poll_socket_cb;
  callbacks_.unregister_poll_socket = &SlirpNetBackend::unregister_poll_socket_cb;
#else
  callbacks_.register_poll_fd = &SlirpNetBackend::register_poll_socket_cb;
  callbacks_.unregister_poll_fd = &SlirpNetBackend::unregister_poll_socket_cb;
#endif
  callbacks_.notify = &SlirpNetBackend::notify_cb;

  slirp_ = slirp_new(&cfg, &callbacks_, this);
}

SlirpNetBackend::~SlirpNetBackend() {
  if (slirp_ != nullptr) {
    slirp_cleanup(slirp_);
    slirp_ = nullptr;
  }
  while (!timers_.empty()) {
    delete timers_.back();
    timers_.pop_back();
  }
}

void SlirpNetBackend::send_guest_frame(std::vector<std::uint8_t> frame) {
  if (slirp_ == nullptr || frame.empty()) {
    return;
  }
  process_expired_timers();
  slirp_input(slirp_, frame.data(), static_cast<int>(frame.size()));
}

bool SlirpNetBackend::poll(std::uint32_t max_wait_ms) {
  if (slirp_ == nullptr) {
    return false;
  }

  const bool had_activity = consume_activity_flag();
  process_expired_timers();

  std::vector<pollfd> pollfds;
  std::uint32_t timeout = max_wait_ms;
#if SLIRP_CHECK_VERSION(4, 9, 0)
  slirp_pollfds_fill_socket(slirp_, &timeout, &SlirpNetBackend::add_poll_fd_cb, &pollfds);
#else
  slirp_pollfds_fill(slirp_, &timeout, &SlirpNetBackend::add_poll_fd_cb, &pollfds);
#endif

  int poll_timeout = 0;
  if (max_wait_ms != 0u) {
    if (timeout == UINT32_MAX) {
      timeout = max_wait_ms;
    }
    timeout = std::min(timeout, max_wait_ms);
    poll_timeout = static_cast<int>(std::min<std::uint32_t>(timeout,
                                                            static_cast<std::uint32_t>(std::numeric_limits<int>::max())));
  }

  if (!pollfds.empty() || poll_timeout > 0) {
    pollfd* pollfd_ptr = pollfds.empty() ? nullptr : pollfds.data();
    const int poll_result = ::poll(pollfd_ptr, static_cast<nfds_t>(pollfds.size()), poll_timeout);
    if (!pollfds.empty()) {
      slirp_pollfds_poll(slirp_, poll_result < 0, &SlirpNetBackend::get_revents_cb, &pollfds);
    }
  } else if (!pollfds.empty()) {
    const int poll_result = ::poll(pollfds.data(), static_cast<nfds_t>(pollfds.size()), 0);
    slirp_pollfds_poll(slirp_, poll_result < 0, &SlirpNetBackend::get_revents_cb, &pollfds);
  }

  process_expired_timers();
  return had_activity || consume_activity_flag();
}

bool SlirpNetBackend::save_state(std::ostream& out) const {
  if (slirp_ == nullptr) {
    return false;
  }

  const std::int32_t version = static_cast<std::int32_t>(slirp_state_version());
  std::vector<std::uint8_t> bytes;
  if (slirp_state_save(slirp_, &SlirpNetBackend::save_state_cb, &bytes) != 0) {
    return false;
  }

  return snapshot_io::write(out, version) &&
         snapshot_io::write_vector(out, bytes);
}

bool SlirpNetBackend::load_state(std::istream& in) {
  if (slirp_ == nullptr) {
    return false;
  }

  std::int32_t version = 0;
  std::vector<std::uint8_t> bytes;
  if (!snapshot_io::read(in, version) ||
      !snapshot_io::read_vector(in, bytes)) {
    return false;
  }

  StateLoadCursor cursor{
      .data = bytes.data(),
      .size = bytes.size(),
      .offset = 0,
  };
  const int ret = slirp_state_load(slirp_, version, &SlirpNetBackend::load_state_cb, &cursor);
  return ret == 0 && cursor.offset == cursor.size;
}

std::int64_t SlirpNetBackend::clock_now_ns() const {
  using clock = std::chrono::steady_clock;
  return std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now().time_since_epoch()).count();
}

std::int64_t SlirpNetBackend::clock_now_ms() const {
  return clock_now_ns() / 1000000;
}

void SlirpNetBackend::process_expired_timers() {
  if (slirp_ == nullptr || timers_.empty()) {
    return;
  }

  const std::int64_t now_ms = clock_now_ms();
  std::vector<Timer*> expired;
  expired.reserve(timers_.size());
  for (Timer* timer : timers_) {
    if (timer->armed && timer->expire_time_ms <= now_ms) {
      timer->armed = false;
      expired.push_back(timer);
    }
  }

  for (Timer* timer : expired) {
#if SLIRP_CHECK_VERSION(4, 7, 0)
    slirp_handle_timer(slirp_, timer->id, timer->cb_opaque);
#else
    if (timer->cb != nullptr) {
      timer->cb(timer->cb_opaque);
    }
#endif
  }
}

void SlirpNetBackend::erase_timer(Timer* timer) {
  std::erase(timers_, timer);
}

bool SlirpNetBackend::consume_activity_flag() {
  const bool active = activity_pending_;
  activity_pending_ = false;
  return active;
}

slirp_ssize_t SlirpNetBackend::send_packet_cb(const void* buf, std::size_t len, void* opaque) {
  auto* self = static_cast<SlirpNetBackend*>(opaque);
  self->activity_pending_ = true;
  if (self->rx_frame_callback_ != nullptr) {
    const auto* bytes = static_cast<const std::uint8_t*>(buf);
    self->rx_frame_callback_(std::vector<std::uint8_t>(bytes, bytes + len));
  }
  return static_cast<slirp_ssize_t>(len);
}

void SlirpNetBackend::guest_error_cb(const char* msg, void* opaque) {
  static_cast<SlirpNetBackend*>(opaque)->activity_pending_ = true;
  std::cerr << "slirp guest error: " << msg << '\n';
}

std::int64_t SlirpNetBackend::clock_get_ns_cb(void* opaque) {
  return static_cast<SlirpNetBackend*>(opaque)->clock_now_ns();
}

#if SLIRP_CHECK_VERSION(4, 7, 0)
void* SlirpNetBackend::timer_new_opaque_cb(SlirpTimerId id, void* cb_opaque, void* opaque) {
  auto* self = static_cast<SlirpNetBackend*>(opaque);
  auto* timer = new Timer{
      .id = id,
      .cb_opaque = cb_opaque,
  };
  self->timers_.push_back(timer);
  return timer;
}
#else
void* SlirpNetBackend::timer_new_cb(SlirpTimerCb cb, void* cb_opaque, void* opaque) {
  auto* self = static_cast<SlirpNetBackend*>(opaque);
  auto* timer = new Timer{
      .cb = cb,
      .cb_opaque = cb_opaque,
  };
  self->timers_.push_back(timer);
  return timer;
}
#endif

void SlirpNetBackend::timer_free_cb(void* timer, void* opaque) {
  auto* self = static_cast<SlirpNetBackend*>(opaque);
  auto* typed_timer = timer_from_handle(timer);
  self->erase_timer(typed_timer);
  delete typed_timer;
}

void SlirpNetBackend::timer_mod_cb(void* timer, std::int64_t expire_time, void* opaque) {
  auto* self = static_cast<SlirpNetBackend*>(opaque);
  auto* typed_timer = timer_from_handle(timer);
  typed_timer->armed = true;
  typed_timer->expire_time_ms = expire_time;
  self->activity_pending_ = true;
}

#if SLIRP_CHECK_VERSION(4, 9, 0)
void SlirpNetBackend::register_poll_socket_cb(slirp_os_socket, void*) {}
void SlirpNetBackend::unregister_poll_socket_cb(slirp_os_socket, void*) {}
#else
void SlirpNetBackend::register_poll_socket_cb(int, void*) {}
void SlirpNetBackend::unregister_poll_socket_cb(int, void*) {}
#endif

void SlirpNetBackend::notify_cb(void* opaque) {
  static_cast<SlirpNetBackend*>(opaque)->activity_pending_ = true;
}

int SlirpNetBackend::add_poll_fd_cb(
#if SLIRP_CHECK_VERSION(4, 9, 0)
    slirp_os_socket
#else
    int
#endif
        fd,
    int events,
    void* opaque) {
  auto* pollfds = static_cast<std::vector<pollfd>*>(opaque);
  pollfds->push_back(pollfd{
      .fd = fd,
      .events = poll_events_from_slirp(events),
      .revents = 0,
  });
  return static_cast<int>(pollfds->size() - 1);
}

int SlirpNetBackend::get_revents_cb(int index, void* opaque) {
  auto* pollfds = static_cast<std::vector<pollfd>*>(opaque);
  if (index < 0 || static_cast<std::size_t>(index) >= pollfds->size()) {
    return 0;
  }
  return slirp_events_from_poll((*pollfds)[static_cast<std::size_t>(index)].revents);
}

slirp_ssize_t SlirpNetBackend::save_state_cb(const void* buf, std::size_t len, void* opaque) {
  auto* bytes = static_cast<std::vector<std::uint8_t>*>(opaque);
  const auto* begin = static_cast<const std::uint8_t*>(buf);
  bytes->insert(bytes->end(), begin, begin + len);
  return static_cast<slirp_ssize_t>(len);
}

slirp_ssize_t SlirpNetBackend::load_state_cb(void* buf, std::size_t len, void* opaque) {
  auto* cursor = static_cast<StateLoadCursor*>(opaque);
  if (cursor->offset + len > cursor->size) {
    return -1;
  }
  std::memcpy(buf, cursor->data + cursor->offset, len);
  cursor->offset += len;
  return static_cast<slirp_ssize_t>(len);
}

} // namespace aarchvm
