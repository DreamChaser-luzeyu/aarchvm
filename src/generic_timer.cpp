#include "aarchvm/generic_timer.hpp"

#include <cstdint>

namespace aarchvm {

std::uint64_t GenericTimer::read(std::uint64_t offset, std::size_t size) {
  if (size != 8 && size != 4) {
    return 0;
  }

  if (offset == 0x00) {
    return counter_;
  }
  if (offset == 0x08) {
    return compare_;
  }
  if (offset == 0x10) {
    return (enabled_ ? 1ull : 0ull) | (irq_pending_ ? 2ull : 0ull);
  }
  return 0;
}

void GenericTimer::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  if (size != 8 && size != 4) {
    return;
  }

  if (offset == 0x08) {
    compare_ = value;
    fired_ = false;
  } else if (offset == 0x10) {
    enabled_ = (value & 1u) != 0;
    if (!enabled_) {
      fired_ = false;
    }
    if ((value & 2u) != 0) {
      irq_pending_ = false;
    }
  }
}

void GenericTimer::tick(std::uint64_t cycles) {
  counter_ += cycles;
  if (enabled_ && !irq_pending_ && !fired_ && counter_ >= compare_) {
    irq_pending_ = true;
    fired_ = true;
  }
  if (cntv_enable_ && !cntv_imask_ && !cntv_fired_ && counter_ >= cntv_cval_el0_) {
    irq_pending_ = true;
    cntv_fired_ = true;
  }
}

std::uint64_t GenericTimer::read_cntv_ctl_el0() const {
  const bool istatus = cntv_enable_ && (counter_ >= cntv_cval_el0_);
  return (cntv_enable_ ? 1ull : 0ull) |
         (cntv_imask_ ? 2ull : 0ull) |
         (istatus ? 4ull : 0ull);
}

void GenericTimer::write_cntv_ctl_el0(std::uint64_t value) {
  cntv_enable_ = (value & 1u) != 0;
  cntv_imask_ = (value & 2u) != 0;
  if (!cntv_enable_) {
    cntv_fired_ = false;
  }
}

void GenericTimer::write_cntv_cval_el0(std::uint64_t value) {
  cntv_cval_el0_ = value;
  cntv_fired_ = false;
}

std::uint64_t GenericTimer::read_cntv_tval_el0() const {
  const std::int64_t delta = static_cast<std::int64_t>(cntv_cval_el0_ - counter_);
  return static_cast<std::uint32_t>(delta);
}

void GenericTimer::write_cntv_tval_el0(std::uint64_t value) {
  const std::int64_t sval = static_cast<std::int32_t>(value & 0xFFFFFFFFu);
  cntv_cval_el0_ = static_cast<std::uint64_t>(static_cast<std::int64_t>(counter_) + sval);
  cntv_fired_ = false;
}

} // namespace aarchvm
