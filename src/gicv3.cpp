#include "aarchvm/gicv3.hpp"

#include "aarchvm/snapshot_io.hpp"

#include <cstdlib>
#include <iostream>

namespace aarchvm {

namespace {

bool env_flag_enabled(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr && value[0] != '\0';
}

bool gic_trace_enabled() {
  static const bool enabled = env_flag_enabled("AARCHVM_TRACE_GIC");
  return enabled;
}

bool gic_pending_trace_enabled() {
  static const bool enabled = env_flag_enabled("AARCHVM_TRACE_GIC_PENDING");
  return enabled;
}

void trace_gic(const char* tag, std::uint32_t intid, bool enable) {
  if (!gic_trace_enabled()) {
    return;
  }
  if (intid == 11u || intid == 14u || intid == 27u || intid == 30u || intid == 33u) {
    std::cerr << std::dec << tag << " intid=" << intid << " val=" << (enable ? 1 : 0) << '\n';
  }
}

constexpr std::uint32_t kGicdCtlr = 0x0000;
constexpr std::uint32_t kGicdTyper = 0x0004;
constexpr std::uint32_t kGicdIidr = 0x0008;
constexpr std::uint32_t kGicdIsenabler = 0x0100;
constexpr std::uint32_t kGicdIcenabler = 0x0180;
constexpr std::uint32_t kGicdIsactiver = 0x0300;
constexpr std::uint32_t kGicdIcactiver = 0x0380;
constexpr std::uint32_t kGicdIpriorityr = 0x0400;
constexpr std::uint32_t kGicdIrouter = 0x6000;
constexpr std::uint32_t kGicdPidr2 = 0xFFE8;

constexpr std::uint32_t kGicrTyper = 0x0008;
constexpr std::uint32_t kGicrWaker = 0x0014;
constexpr std::uint32_t kGicrPidr2 = 0xFFE8;
constexpr std::uint32_t kGicrSgiIgroupr0 = 0x0B0080u;
constexpr std::uint32_t kGicrSgiIsenabler0 = 0x0B0100u;
constexpr std::uint32_t kGicrSgiIcenabler0 = 0x0B0180u;
constexpr std::uint32_t kGicrSgiIspendr0 = 0x0B0200u;
constexpr std::uint32_t kGicrSgiIcpendr0 = 0x0B0280u;
constexpr std::uint32_t kGicrSgiIsactiver0 = 0x0B0300u;
constexpr std::uint32_t kGicrSgiIcactiver0 = 0x0B0380u;
constexpr std::uint32_t kGicrSgiIpriorityr = 0x0B0400u;
constexpr std::uint32_t kGicrSgiIcfgr0 = 0x0B0C00u;
constexpr std::uint32_t kGicrSgiIcfgr1 = 0x0B0C04u;

constexpr std::uint32_t kGicArchRevGicv3 = 0x3u << 4;
constexpr std::uint8_t kDefaultPriority = 0xC0u;

bool is_write1_bitmap_register(std::uint64_t offset) {
  return (offset >= kGicdIsenabler && offset < (kGicdIsenabler + 0x80u)) ||
         (offset >= kGicdIcenabler && offset < (kGicdIcenabler + 0x80u)) ||
         (offset >= kGicdIsactiver && offset < (kGicdIsactiver + 0x80u)) ||
         (offset >= kGicdIcactiver && offset < (kGicdIcactiver + 0x80u)) ||
         (offset >= kGicrSgiIsenabler0 && offset < (kGicrSgiIsenabler0 + 4u)) ||
         (offset >= kGicrSgiIcenabler0 && offset < (kGicrSgiIcenabler0 + 4u)) ||
         (offset >= kGicrSgiIspendr0 && offset < (kGicrSgiIspendr0 + 4u)) ||
         (offset >= kGicrSgiIcpendr0 && offset < (kGicrSgiIcpendr0 + 4u)) ||
         (offset >= kGicrSgiIsactiver0 && offset < (kGicrSgiIsactiver0 + 4u)) ||
         (offset >= kGicrSgiIcactiver0 && offset < (kGicrSgiIcactiver0 + 4u));
}

std::uint32_t read_part(std::uint32_t value, std::uint64_t offset, std::size_t size) {
  const std::uint32_t shift = static_cast<std::uint32_t>((offset & 0x3u) * 8u);
  if (size == 1u) {
    return (value >> shift) & 0xFFu;
  }
  if (size == 2u) {
    return (value >> shift) & 0xFFFFu;
  }
  return value;
}

} // namespace

GicV3::GicV3() {
  enabled_.fill(false);
  priorities_.fill(kDefaultPriority);
  rebuild_bitmaps();
}

std::uint64_t GicV3::read(std::uint64_t offset, std::size_t size) {
  if (size != 1u && size != 2u && size != 4u && size != 8u) {
    return 0;
  }

  const std::uint32_t value32 =
      (offset < kDistSize) ? read_dist_reg32(offset & ~0x3ull) : read_redist_reg32(offset & ~0x3ull);
  if (size == 8u) {
    const std::uint64_t lo = value32;
    const std::uint64_t hi =
        (offset < kDistSize) ? read_dist_reg32((offset & ~0x7ull) + 4u) : read_redist_reg32((offset & ~0x7ull) + 4u);
    return lo | (hi << 32u);
  }
  return read_part(value32, offset, size);
}

void GicV3::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  if (size != 1u && size != 2u && size != 4u && size != 8u) {
    return;
  }

  const auto write32 = [&](std::uint64_t off, std::uint32_t val) {
    if (off < kDistSize) {
      write_dist_reg32(off, val);
    } else {
      write_redist_reg32(off, val);
    }
  };

  if (size == 8u) {
    write32(offset & ~0x7ull, static_cast<std::uint32_t>(value & 0xFFFFFFFFu));
    write32((offset & ~0x7ull) + 4u, static_cast<std::uint32_t>((value >> 32u) & 0xFFFFFFFFu));
    return;
  }

  if (size != 4u) {
    const std::uint64_t aligned = offset & ~0x3ull;
    const std::uint32_t shift = static_cast<std::uint32_t>((offset & 0x3u) * 8u);
    const std::uint32_t mask = (size == 1u) ? 0xFFu : 0xFFFFu;
    const std::uint32_t part = (static_cast<std::uint32_t>(value) & mask) << shift;
    if (is_write1_bitmap_register(aligned)) {
      write32(aligned, part);
      return;
    }
    if ((offset & 0x3u) != 0u) {
      std::uint32_t cur = (aligned < kDistSize) ? read_dist_reg32(aligned) : read_redist_reg32(aligned);
      cur = (cur & ~(mask << shift)) | part;
      write32(aligned, cur);
      return;
    }
  }

  write32(offset, static_cast<std::uint32_t>(value));
}

std::uint32_t GicV3::read_dist_reg32(std::uint64_t offset) const {
  if (offset == kGicdCtlr) {
    return gicd_ctlr_;
  }
  if (offset == kGicdTyper) {
    return 1u;
  }
  if (offset == kGicdIidr) {
    return 0x0102043Bu;
  }
  if (offset >= kGicdIsenabler && offset < (kGicdIsenabler + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIsenabler) / 4u);
    return (n == 0u) ? 0u : get_enable_range(n * 32u);
  }
  if (offset >= kGicdIcenabler && offset < (kGicdIcenabler + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIcenabler) / 4u);
    return (n == 0u) ? 0u : get_enable_range(n * 32u);
  }
  if (offset >= kGicdIsactiver && offset < (kGicdIsactiver + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIsactiver) / 4u);
    return (n == 0u) ? 0u : get_active_range(n * 32u);
  }
  if (offset >= kGicdIcactiver && offset < (kGicdIcactiver + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIcactiver) / 4u);
    return (n == 0u) ? 0u : get_active_range(n * 32u);
  }
  if (offset >= kGicdIpriorityr && offset < (kGicdIpriorityr + kNumIntIds)) {
    const std::uint32_t first = static_cast<std::uint32_t>(offset - kGicdIpriorityr);
    return get_priority_range(first);
  }
  if (offset >= kGicdIrouter && offset < (kGicdIrouter + 8u * 988u)) {
    return 0;
  }
  if (offset == kGicdPidr2) {
    return kGicArchRevGicv3;
  }
  return 0;
}

std::uint32_t GicV3::read_redist_reg32(std::uint64_t offset) const {
  if (offset == (kRedistBase + kGicrTyper)) {
    return 1u << 4;
  }
  if (offset == (kRedistBase + kGicrWaker)) {
    return gicr_waker_ & ~0x4u;
  }
  if (offset == (kRedistBase + kGicrPidr2)) {
    return kGicArchRevGicv3;
  }
  if (offset == (kRedistSgiBase + 0x0008u)) {
    return 1u << 4;
  }
  if (offset == kGicrSgiIgroupr0) {
    return gicr_igroupr0_;
  }
  if (offset == kGicrSgiIsenabler0 || offset == kGicrSgiIcenabler0) {
    return get_enable_range(0u);
  }
  if (offset == kGicrSgiIspendr0 || offset == kGicrSgiIcpendr0) {
    return get_pending_range(0u);
  }
  if (offset == kGicrSgiIsactiver0 || offset == kGicrSgiIcactiver0) {
    return get_active_range(0u);
  }
  if (offset >= kGicrSgiIpriorityr && offset < (kGicrSgiIpriorityr + 32u)) {
    return get_priority_range(static_cast<std::uint32_t>(offset - kGicrSgiIpriorityr));
  }
  if (offset == kGicrSgiIcfgr0) {
    return gicr_icfgr0_;
  }
  if (offset == kGicrSgiIcfgr1) {
    return gicr_icfgr1_;
  }
  return 0;
}

void GicV3::write_dist_reg32(std::uint64_t offset, std::uint32_t value) {
  if (offset == kGicdCtlr) {
    gicd_ctlr_ = value & 0x13u;
    return;
  }
  if (offset >= kGicdIsenabler && offset < (kGicdIsenabler + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIsenabler) / 4u);
    if (n != 0u) {
      set_enable_range(n * 32u, value, true);
    }
    return;
  }
  if (offset >= kGicdIcenabler && offset < (kGicdIcenabler + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIcenabler) / 4u);
    if (n != 0u) {
      set_enable_range(n * 32u, value, false);
    }
    return;
  }
  if (offset >= kGicdIsactiver && offset < (kGicdIsactiver + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIsactiver) / 4u);
    if (n != 0u) {
      set_active_range(n * 32u, value, true);
    }
    return;
  }
  if (offset >= kGicdIcactiver && offset < (kGicdIcactiver + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIcactiver) / 4u);
    if (n != 0u) {
      set_active_range(n * 32u, value, false);
    }
    return;
  }
  if (offset >= kGicdIpriorityr && offset < (kGicdIpriorityr + kNumIntIds)) {
    const std::uint32_t first = static_cast<std::uint32_t>(offset - kGicdIpriorityr);
    set_priority_range(first, value, 0u, 4u);
    return;
  }
}

void GicV3::write_redist_reg32(std::uint64_t offset, std::uint32_t value) {
  if (offset == (kRedistBase + kGicrWaker)) {
    gicr_waker_ = value & 0x2u;
    return;
  }
  if (offset == kGicrSgiIgroupr0) {
    gicr_igroupr0_ = value;
    return;
  }
  if (offset == kGicrSgiIsenabler0) {
    set_enable_range(0u, value, true);
    return;
  }
  if (offset == kGicrSgiIcenabler0) {
    set_enable_range(0u, value, false);
    return;
  }
  if (offset == kGicrSgiIspendr0) {
    set_pending_range(0u, value, true);
    return;
  }
  if (offset == kGicrSgiIcpendr0) {
    set_pending_range(0u, value, false);
    return;
  }
  if (offset == kGicrSgiIsactiver0) {
    set_active_range(0u, value, true);
    return;
  }
  if (offset == kGicrSgiIcactiver0) {
    set_active_range(0u, value, false);
    return;
  }
  if (offset >= kGicrSgiIpriorityr && offset < (kGicrSgiIpriorityr + 32u)) {
    set_priority_range(static_cast<std::uint32_t>(offset - kGicrSgiIpriorityr), value, 0u, 4u);
    return;
  }
  if (offset == kGicrSgiIcfgr0) {
    gicr_icfgr0_ = value;
    return;
  }
  if (offset == kGicrSgiIcfgr1) {
    gicr_icfgr1_ = value;
    return;
  }
}

void GicV3::refresh_word(std::uint32_t word_index_value) {
  if (word_index_value >= kWords) {
    return;
  }

  const std::uint32_t first = word_index_value * 64u;
  std::uint64_t pending_word = 0;
  std::uint64_t enabled_word = 0;
  std::uint64_t active_word = 0;
  for (std::uint32_t bit = 0; bit < 64u; ++bit) {
    const std::uint32_t intid = first + bit;
    if (intid >= kNumIntIds) {
      break;
    }
    if (pending_[intid]) {
      pending_word |= (1ull << bit);
    }
    if (enabled_[intid]) {
      enabled_word |= (1ull << bit);
    }
    if (active_[intid]) {
      active_word |= (1ull << bit);
    }
  }
  pending_bits_[word_index_value] = pending_word;
  enabled_bits_[word_index_value] = enabled_word;
  active_bits_[word_index_value] = active_word;
  pending_enabled_bits_[word_index_value] = pending_word & enabled_word & ~active_word;
}

void GicV3::rebuild_bitmaps() {
  pending_bits_.fill(0);
  enabled_bits_.fill(0);
  active_bits_.fill(0);
  pending_enabled_bits_.fill(0);
  for (std::uint32_t word = 0; word < kWords; ++word) {
    refresh_word(word);
  }
  recompute_best_pending();
}

bool GicV3::is_candidate(std::uint32_t intid) const {
  return intid < kNumIntIds && pending_[intid] && enabled_[intid] && !active_[intid];
}

void GicV3::consider_best_pending(std::uint32_t intid) {
  if (!is_candidate(intid)) {
    return;
  }
  const std::uint8_t prio = priorities_[intid];
  if (!best_pending_valid_ || prio < best_pending_priority_ ||
      (prio == best_pending_priority_ && intid < best_pending_intid_)) {
    best_pending_valid_ = true;
    best_pending_intid_ = intid;
    best_pending_priority_ = prio;
  }
}

void GicV3::recompute_best_pending() {
  ++perf_counters_.recompute_calls;
  best_pending_valid_ = false;
  best_pending_intid_ = 1023u;
  best_pending_priority_ = 0xFFu;
  for (std::uint32_t intid = 0; intid < kNumIntIds; ++intid) {
    if (!pending_[intid] || !enabled_[intid] || active_[intid]) {
      continue;
    }
    const std::uint8_t prio = priorities_[intid];
    if (!best_pending_valid_ || prio < best_pending_priority_ ||
        (prio == best_pending_priority_ && intid < best_pending_intid_)) {
      best_pending_valid_ = true;
      best_pending_intid_ = intid;
      best_pending_priority_ = prio;
    }
  }
}

void GicV3::set_pending_bit(std::uint32_t intid, bool value) {
  if (intid >= kNumIntIds || pending_[intid] == value) {
    return;
  }
  if (gic_pending_trace_enabled() && (intid == 21u || intid == 27u || intid == 33u)) {
    std::cerr << std::dec << "GIC-PEND intid=" << intid << " val=" << (value ? 1 : 0) << '\n';
  }
  const bool was_best = best_pending_valid_ && best_pending_intid_ == intid;
  pending_[intid] = value;
  ++state_epoch_;
  refresh_word(word_index(intid));
  if (value) {
    consider_best_pending(intid);
  } else if (was_best) {
    recompute_best_pending();
  }
}

void GicV3::set_enabled_bit(std::uint32_t intid, bool value) {
  if (intid >= kNumIntIds || enabled_[intid] == value) {
    return;
  }
  const bool was_best = best_pending_valid_ && best_pending_intid_ == intid;
  enabled_[intid] = value;
  ++state_epoch_;
  refresh_word(word_index(intid));
  if (value) {
    consider_best_pending(intid);
  } else if (was_best) {
    recompute_best_pending();
  }
}

void GicV3::set_active_bit(std::uint32_t intid, bool value) {
  if (intid >= kNumIntIds || active_[intid] == value) {
    return;
  }
  const bool was_best = best_pending_valid_ && best_pending_intid_ == intid;
  active_[intid] = value;
  ++state_epoch_;
  refresh_word(word_index(intid));
  if (!value) {
    consider_best_pending(intid);
  } else if (was_best) {
    recompute_best_pending();
  }
}

void GicV3::set_enable_range(std::uint32_t first_intid, std::uint32_t bits, bool enable) {
  for (std::uint32_t bit = 0; bit < 32u; ++bit) {
    const std::uint32_t intid = first_intid + bit;
    if (intid >= kNumIntIds) {
      break;
    }
    if (((bits >> bit) & 1u) != 0u) {
      enabled_[intid] = enable;
      trace_gic("GIC-ENABLE", intid, enable);
    }
  }
  refresh_word(word_index(first_intid));
  recompute_best_pending();
}

std::uint32_t GicV3::get_enable_range(std::uint32_t first_intid) const {
  std::uint32_t bits = 0;
  for (std::uint32_t bit = 0; bit < 32u; ++bit) {
    const std::uint32_t intid = first_intid + bit;
    if (intid >= kNumIntIds) {
      break;
    }
    if (enabled_[intid]) {
      bits |= (1u << bit);
    }
  }
  return bits;
}

void GicV3::set_pending_range(std::uint32_t first_intid, std::uint32_t bits, bool value) {
  for (std::uint32_t bit = 0; bit < 32u; ++bit) {
    const std::uint32_t intid = first_intid + bit;
    if (intid >= kNumIntIds) {
      break;
    }
    if (((bits >> bit) & 1u) != 0u) {
      set_pending_bit(intid, value);
    }
  }
}

std::uint32_t GicV3::get_pending_range(std::uint32_t first_intid) const {
  std::uint32_t bits = 0;
  for (std::uint32_t bit = 0; bit < 32u; ++bit) {
    const std::uint32_t intid = first_intid + bit;
    if (intid >= kNumIntIds) {
      break;
    }
    if (pending_[intid]) {
      bits |= (1u << bit);
    }
  }
  return bits;
}

void GicV3::set_active_range(std::uint32_t first_intid, std::uint32_t bits, bool value) {
  for (std::uint32_t bit = 0; bit < 32u; ++bit) {
    const std::uint32_t intid = first_intid + bit;
    if (intid >= kNumIntIds) {
      break;
    }
    if (((bits >> bit) & 1u) != 0u) {
      set_active_bit(intid, value);
    }
  }
}

std::uint32_t GicV3::get_active_range(std::uint32_t first_intid) const {
  std::uint32_t bits = 0;
  for (std::uint32_t bit = 0; bit < 32u; ++bit) {
    const std::uint32_t intid = first_intid + bit;
    if (intid >= kNumIntIds) {
      break;
    }
    if (active_[intid]) {
      bits |= (1u << bit);
    }
  }
  return bits;
}

std::uint32_t GicV3::get_priority_range(std::uint32_t first_intid) const {
  std::uint32_t value = 0;
  for (std::uint32_t i = 0; i < 4u; ++i) {
    const std::uint32_t intid = first_intid + i;
    const std::uint8_t prio = (intid < kNumIntIds) ? priorities_[intid] : kDefaultPriority;
    value |= static_cast<std::uint32_t>(prio) << (i * 8u);
  }
  return value;
}

void GicV3::set_priority_range(std::uint32_t first_intid,
                               std::uint32_t value,
                               std::uint32_t start_byte,
                               std::uint32_t count) {
  bool need_recompute = false;
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::uint32_t intid = first_intid + start_byte + i;
    if (intid >= kNumIntIds) {
      break;
    }
    const std::uint8_t old_prio = priorities_[intid];
    const std::uint8_t new_prio = static_cast<std::uint8_t>((value >> (i * 8u)) & 0xFFu);
    if (old_prio == new_prio) {
      continue;
    }
    priorities_[intid] = new_prio;
    ++state_epoch_;
    if (best_pending_valid_ && best_pending_intid_ == intid) {
      need_recompute = true;
    } else {
      consider_best_pending(intid);
    }
  }
  if (need_recompute) {
    recompute_best_pending();
  }
}

void GicV3::set_pending(std::uint32_t intid) {
  set_pending_bit(intid, true);
  if (!best_pending_valid_) {
    ++state_epoch_;
  }
}

void GicV3::set_level(std::uint32_t intid, bool asserted) {
  ++perf_counters_.set_level_calls;
  if (intid >= kNumIntIds || line_level_[intid] == asserted) {
    return;
  }
  line_level_[intid] = asserted;
  ++state_epoch_;
  if (asserted && !active_[intid]) {
    set_pending_bit(intid, true);
  }
}

bool GicV3::has_pending() const {
  ++perf_counters_.has_pending_calls;
  return best_pending_valid_;
}

bool GicV3::has_pending(std::uint8_t pmr) const {
  ++perf_counters_.has_pending_calls;
  return best_pending_valid_ && best_pending_priority_ < pmr;
}

bool GicV3::acknowledge(std::uint32_t& intid) {
  ++perf_counters_.acknowledge_calls;
  for (std::uint32_t word = 0; word < kWords; ++word) {
    const std::uint64_t bits = pending_enabled_bits_[word];
    if (bits == 0u) {
      continue;
    }
    const std::uint32_t bit = static_cast<std::uint32_t>(__builtin_ctzll(bits));
    intid = word * 64u + bit;
    set_pending_bit(intid, false);
    set_active_bit(intid, true);
    return true;
  }
  intid = 1023u;
  return false;
}

bool GicV3::acknowledge(std::uint8_t pmr, std::uint32_t& intid) {
  ++perf_counters_.acknowledge_calls;
  if (!best_pending_valid_ || best_pending_priority_ >= pmr) {
    intid = 1023u;
    return false;
  }
  intid = best_pending_intid_;
  set_pending_bit(intid, false);
  set_active_bit(intid, true);
  return true;
}

void GicV3::eoi(std::uint32_t intid) {
  if (intid >= kNumIntIds) {
    return;
  }
  set_active_bit(intid, false);
  if (line_level_[intid]) {
    set_pending_bit(intid, true);
  }
}

bool GicV3::pending(std::uint32_t intid) const {
  if (intid >= kNumIntIds) {
    return false;
  }
  return (pending_bits_[word_index(intid)] & bit_mask(intid)) != 0u;
}

bool GicV3::enabled(std::uint32_t intid) const {
  if (intid >= kNumIntIds) {
    return false;
  }
  return (enabled_bits_[word_index(intid)] & bit_mask(intid)) != 0u;
}

std::uint8_t GicV3::priority(std::uint32_t intid) const {
  if (intid >= kNumIntIds) {
    return kDefaultPriority;
  }
  return priorities_[intid];
}

bool GicV3::save_state(std::ostream& out) const {
  return snapshot_io::write_array(out, pending_) &&
         snapshot_io::write_array(out, enabled_) &&
         snapshot_io::write_array(out, active_) &&
         snapshot_io::write_array(out, line_level_) &&
         snapshot_io::write_array(out, priorities_) &&
         snapshot_io::write(out, gicd_ctlr_) &&
         snapshot_io::write(out, gicr_waker_) &&
         snapshot_io::write(out, gicr_igroupr0_) &&
         snapshot_io::write(out, gicr_icfgr0_) &&
         snapshot_io::write(out, gicr_icfgr1_);
}

bool GicV3::load_state(std::istream& in, std::uint32_t version) {
  active_.fill(false);
  line_level_.fill(false);
  priorities_.fill(kDefaultPriority);
  gicr_igroupr0_ = 0xFFFFFFFFu;
  gicr_icfgr0_ = 0xAAAAAAAAu;
  gicr_icfgr1_ = 0;
  if (!snapshot_io::read_array(in, pending_) ||
      !snapshot_io::read_array(in, enabled_)) {
    return false;
  }
  if (version >= 3) {
    if (!snapshot_io::read_array(in, active_)) {
      return false;
    }
    if (version >= 4) {
      if (!snapshot_io::read_array(in, line_level_)) {
        return false;
      }
    }
    if (!snapshot_io::read_array(in, priorities_)) {
      return false;
    }
  }
  if (!snapshot_io::read(in, gicd_ctlr_) ||
      !snapshot_io::read(in, gicr_waker_)) {
    return false;
  }
  if (version >= 3) {
    if (!snapshot_io::read(in, gicr_igroupr0_) ||
        !snapshot_io::read(in, gicr_icfgr0_) ||
        !snapshot_io::read(in, gicr_icfgr1_)) {
      return false;
    }
  }
  rebuild_bitmaps();
  return true;
}

} // namespace aarchvm
