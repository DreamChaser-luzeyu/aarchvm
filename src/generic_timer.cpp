#include "aarchvm/generic_timer.hpp"

#include "aarchvm/snapshot_io.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

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
    update_irq_state();
  } else if (offset == 0x10) {
    enabled_ = (value & 1u) != 0;
    if (!enabled_) {
      fired_ = false;
    }
    if ((value & 2u) != 0) {
      irq_pending_ = false;
    }
    update_irq_state();
  }
}

void GenericTimer::tick(std::uint64_t cycles) {
  counter_ += cycles;
  update_irq_state();
}

std::uint64_t GenericTimer::counter_at_steps(std::uint64_t steps) const {
  return counter_ + (steps - step_anchor_) * cycles_per_step_;
}

void GenericTimer::sync_to_steps(std::uint64_t steps) {
  counter_ = counter_at_steps(steps);
  step_anchor_ = steps;
  update_irq_state();
}

std::uint64_t GenericTimer::steps_until_irq(std::uint64_t steps, std::uint64_t max_steps) const {
  if (irq_pending_ || max_steps == 0u) {
    return 0u;
  }
  if (cycles_per_step_ == 0u) {
    return max_steps;
  }

  const std::uint64_t current = counter_at_steps(steps);
  std::uint64_t best = max_steps;

  const auto update_best = [&](bool active, bool already_fired, std::uint64_t target) {
    if (!active || already_fired) {
      return;
    }
    if (current >= target) {
      best = 0u;
      return;
    }
    const std::uint64_t cycles = target - current;
    const std::uint64_t steps_needed = (cycles + cycles_per_step_ - 1u) / cycles_per_step_;
    best = std::min(best, steps_needed);
  };

  update_best(enabled_, fired_, compare_);
  update_best(cntv_enable_ && !cntv_imask_, cntv_fired_, cntv_cval_el0_);
  return best;
}

std::uint64_t GenericTimer::read_cntv_ctl_el0(std::uint64_t steps) const {
  const bool istatus = cntv_enable_ && (counter_at_steps(steps) >= cntv_cval_el0_);
  return (cntv_enable_ ? 1ull : 0ull) |
         (cntv_imask_ ? 2ull : 0ull) |
         (istatus ? 4ull : 0ull);
}

void GenericTimer::write_cntv_ctl_el0(std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  cntv_enable_ = (value & 1u) != 0;
  cntv_imask_ = (value & 2u) != 0;
  if (!cntv_enable_) {
    cntv_fired_ = false;
  }
  update_irq_state();
}

void GenericTimer::write_cntv_cval_el0(std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  cntv_cval_el0_ = value;
  cntv_fired_ = false;
  update_irq_state();
}

std::uint64_t GenericTimer::read_cntv_tval_el0(std::uint64_t steps) const {
  const std::int64_t delta = static_cast<std::int64_t>(cntv_cval_el0_ - counter_at_steps(steps));
  return static_cast<std::uint32_t>(delta);
}

void GenericTimer::write_cntv_tval_el0(std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  const std::int64_t sval = static_cast<std::int32_t>(value & 0xFFFFFFFFu);
  cntv_cval_el0_ = static_cast<std::uint64_t>(static_cast<std::int64_t>(counter_) + sval);
  cntv_fired_ = false;
  update_irq_state();
}

void GenericTimer::update_irq_state() {
  if (enabled_ && !irq_pending_ && !fired_ && counter_ >= compare_) {
    irq_pending_ = true;
    fired_ = true;
  }
  if (cntv_enable_ && !cntv_imask_ && !irq_pending_ && !cntv_fired_ && counter_ >= cntv_cval_el0_) {
    irq_pending_ = true;
    cntv_fired_ = true;
  }
}

bool GenericTimer::save_state(std::ostream& out) const {
  return snapshot_io::write(out, counter_) &&
         snapshot_io::write(out, compare_) &&
         snapshot_io::write_bool(out, enabled_) &&
         snapshot_io::write_bool(out, irq_pending_) &&
         snapshot_io::write_bool(out, fired_) &&
         snapshot_io::write(out, cntv_cval_el0_) &&
         snapshot_io::write_bool(out, cntv_enable_) &&
         snapshot_io::write_bool(out, cntv_imask_) &&
         snapshot_io::write_bool(out, cntv_fired_);
}

bool GenericTimer::load_state(std::istream& in) {
  step_anchor_ = 0;
  cycles_per_step_ = 1;
  return snapshot_io::read(in, counter_) &&
         snapshot_io::read(in, compare_) &&
         snapshot_io::read_bool(in, enabled_) &&
         snapshot_io::read_bool(in, irq_pending_) &&
         snapshot_io::read_bool(in, fired_) &&
         snapshot_io::read(in, cntv_cval_el0_) &&
         snapshot_io::read_bool(in, cntv_enable_) &&
         snapshot_io::read_bool(in, cntv_imask_) &&
         snapshot_io::read_bool(in, cntv_fired_);
}

} // namespace aarchvm
