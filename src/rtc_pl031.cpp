#include "aarchvm/rtc_pl031.hpp"

#include "aarchvm/snapshot_io.hpp"

#include <chrono>

namespace aarchvm {

namespace {

constexpr std::uint64_t kRegDr = 0x000;
constexpr std::uint64_t kRegMr = 0x004;
constexpr std::uint64_t kRegLr = 0x008;
constexpr std::uint64_t kRegCr = 0x00c;
constexpr std::uint64_t kRegImsc = 0x010;
constexpr std::uint64_t kRegRis = 0x014;
constexpr std::uint64_t kRegMis = 0x018;
constexpr std::uint64_t kRegIcr = 0x01c;

constexpr std::uint32_t kAlarmBit = 0x1u;

bool valid_mmio_size(std::size_t size) {
  return size == 1 || size == 2 || size == 4 || size == 8;
}

} // namespace

RtcPl031::RtcPl031() {
  reset();
}

std::uint64_t RtcPl031::read(std::uint64_t offset, std::size_t size) {
  if (!valid_mmio_size(size)) {
    return 0;
  }

  refresh_alarm_state();

  switch (offset) {
    case kRegDr:
      return current_seconds();
    case kRegMr:
      return match_seconds_;
    case kRegLr:
      return lr_shadow_;
    case kRegCr:
      return cr_ & 0x1u;
    case kRegImsc:
      return imsc_ & kAlarmBit;
    case kRegRis:
      return raw_pending_ ? kAlarmBit : 0u;
    case kRegMis:
      return raw_pending_ && ((imsc_ & kAlarmBit) != 0u) ? kAlarmBit : 0u;
    case 0xFE0:
      return 0x31u;
    case 0xFE4:
      return 0x10u;
    case 0xFE8:
      return 0x04u;
    case 0xFEC:
      return 0x00u;
    case 0xFF0:
      return 0x0Du;
    case 0xFF4:
      return 0xF0u;
    case 0xFF8:
      return 0x05u;
    case 0xFFC:
      return 0xB1u;
    default:
      return 0;
  }
}

void RtcPl031::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  if (!valid_mmio_size(size)) {
    return;
  }

  const std::uint32_t value32 = static_cast<std::uint32_t>(value);
  switch (offset) {
    case kRegMr:
      match_seconds_ = value32;
      raw_pending_ = false;
      alarm_armed_ = true;
      refresh_alarm_state();
      return;
    case kRegLr:
      lr_shadow_ = value32;
      if (clock_enabled()) {
        offset_seconds_ = static_cast<std::int64_t>(value32) - host_now_seconds();
      } else {
        frozen_seconds_ = value32;
      }
      refresh_alarm_state();
      return;
    case kRegCr: {
      const bool was_enabled = clock_enabled();
      const bool now_enabled = (value32 & 0x1u) != 0u;
      if (was_enabled && !now_enabled) {
        frozen_seconds_ = current_seconds();
      } else if (!was_enabled && now_enabled) {
        offset_seconds_ = static_cast<std::int64_t>(frozen_seconds_) - host_now_seconds();
      }
      cr_ = value32 & 0x1u;
      refresh_alarm_state();
      return;
    }
    case kRegImsc:
      imsc_ = value32 & kAlarmBit;
      return;
    case kRegIcr:
      raw_pending_ = false;
      alarm_armed_ = false;
      return;
    default:
      return;
  }
}

void RtcPl031::reset() {
  offset_seconds_ = 0;
  match_seconds_ = 0;
  cr_ = 0x1u;
  lr_shadow_ = current_seconds();
  imsc_ = 0;
  frozen_seconds_ = lr_shadow_;
  raw_pending_ = false;
  alarm_armed_ = false;
}

bool RtcPl031::save_state(std::ostream& out) const {
  return snapshot_io::write(out, offset_seconds_) &&
         snapshot_io::write(out, match_seconds_) &&
         snapshot_io::write(out, lr_shadow_) &&
         snapshot_io::write(out, cr_) &&
         snapshot_io::write(out, imsc_) &&
         snapshot_io::write(out, frozen_seconds_) &&
         snapshot_io::write_bool(out, raw_pending_) &&
         snapshot_io::write_bool(out, alarm_armed_);
}

bool RtcPl031::load_state(std::istream& in) {
  if (!snapshot_io::read(in, offset_seconds_) ||
      !snapshot_io::read(in, match_seconds_) ||
      !snapshot_io::read(in, lr_shadow_) ||
      !snapshot_io::read(in, cr_) ||
      !snapshot_io::read(in, imsc_) ||
      !snapshot_io::read(in, frozen_seconds_) ||
      !snapshot_io::read_bool(in, raw_pending_) ||
      !snapshot_io::read_bool(in, alarm_armed_)) {
    return false;
  }

  cr_ &= 0x1u;
  imsc_ &= kAlarmBit;
  refresh_alarm_state();
  return true;
}

std::int64_t RtcPl031::host_now_seconds() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::seconds>(now).count());
}

std::uint32_t RtcPl031::current_seconds() const {
  if (!clock_enabled()) {
    return frozen_seconds_;
  }
  return static_cast<std::uint32_t>(host_now_seconds() + offset_seconds_);
}

void RtcPl031::refresh_alarm_state() {
  update_alarm_state(current_seconds());
}

void RtcPl031::update_alarm_state(std::uint32_t now_seconds) {
  if (raw_pending_ || !alarm_armed_ || !clock_enabled()) {
    return;
  }
  if (now_seconds >= match_seconds_) {
    raw_pending_ = true;
    alarm_armed_ = false;
  }
}

} // namespace aarchvm
