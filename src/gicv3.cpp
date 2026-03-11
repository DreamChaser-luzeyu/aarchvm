#include "aarchvm/gicv3.hpp"

#include "aarchvm/snapshot_io.hpp"

namespace aarchvm {

namespace {

constexpr std::uint32_t kGicdCtlr = 0x0000;
constexpr std::uint32_t kGicdTyper = 0x0004;
constexpr std::uint32_t kGicdIidr = 0x0008;
constexpr std::uint32_t kGicdIsenabler = 0x0100;
constexpr std::uint32_t kGicdIcenabler = 0x0180;
constexpr std::uint32_t kGicdIrouter = 0x6000;
constexpr std::uint32_t kGicdPidr2 = 0xFFE8;

constexpr std::uint32_t kGicrTyper = 0x0008;
constexpr std::uint32_t kGicrWaker = 0x0014;
constexpr std::uint32_t kGicrPidr2 = 0xFFE8;
constexpr std::uint32_t kGicrSgiIsenabler0 = 0x0B0080u;
constexpr std::uint32_t kGicrSgiIcenabler0 = 0x0B0180u;

constexpr std::uint32_t kGicArchRevGicv3 = 0x3u << 4;

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
  enabled_.fill(true);
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

  if ((offset & 0x3u) != 0u && size != 4u) {
    const std::uint64_t aligned = offset & ~0x3ull;
    std::uint32_t cur = (aligned < kDistSize) ? read_dist_reg32(aligned) : read_redist_reg32(aligned);
    const std::uint32_t shift = static_cast<std::uint32_t>((offset & 0x3u) * 8u);
    const std::uint32_t mask = (size == 1u) ? 0xFFu : 0xFFFFu;
    cur = (cur & ~(mask << shift)) | ((static_cast<std::uint32_t>(value) & mask) << shift);
    write32(aligned, cur);
    return;
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
    return get_enable_range(n * 32u);
  }
  if (offset >= kGicdIcenabler && offset < (kGicdIcenabler + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIcenabler) / 4u);
    return get_enable_range(n * 32u);
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
    return gicr_waker_ & ~0x6u;
  }
  if (offset == (kRedistBase + kGicrPidr2)) {
    return kGicArchRevGicv3;
  }
  if (offset == (kRedistSgiBase + 0x0008u)) {
    return 1u << 4;
  }
  if (offset >= kGicrSgiIsenabler0 && offset < (kGicrSgiIsenabler0 + 4u)) {
    return get_enable_range(0u);
  }
  if (offset >= kGicrSgiIcenabler0 && offset < (kGicrSgiIcenabler0 + 4u)) {
    return get_enable_range(0u);
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
    set_enable_range(n * 32u, value, true);
    return;
  }
  if (offset >= kGicdIcenabler && offset < (kGicdIcenabler + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIcenabler) / 4u);
    set_enable_range(n * 32u, value, false);
    return;
  }
}

void GicV3::write_redist_reg32(std::uint64_t offset, std::uint32_t value) {
  if (offset == (kRedistBase + kGicrWaker)) {
    gicr_waker_ = value & 0x2u;
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
}

void GicV3::set_enable_range(std::uint32_t first_intid, std::uint32_t bits, bool enable) {
  for (std::uint32_t bit = 0; bit < 32u; ++bit) {
    const std::uint32_t intid = first_intid + bit;
    if (intid >= enabled_.size()) {
      break;
    }
    if (((bits >> bit) & 1u) != 0u) {
      enabled_[intid] = enable;
    }
  }
}

std::uint32_t GicV3::get_enable_range(std::uint32_t first_intid) const {
  std::uint32_t bits = 0;
  for (std::uint32_t bit = 0; bit < 32u; ++bit) {
    const std::uint32_t intid = first_intid + bit;
    if (intid >= enabled_.size()) {
      break;
    }
    if (enabled_[intid]) {
      bits |= (1u << bit);
    }
  }
  return bits;
}

void GicV3::set_pending(std::uint32_t intid) {
  if (intid < pending_.size()) {
    pending_[intid] = true;
  }
}

bool GicV3::has_pending() const {
  for (std::uint32_t i = 0; i < pending_.size(); ++i) {
    if (pending_[i] && enabled_[i]) {
      return true;
    }
  }
  return false;
}

std::optional<std::uint32_t> GicV3::acknowledge() {
  for (std::uint32_t i = 0; i < pending_.size(); ++i) {
    if (pending_[i] && enabled_[i]) {
      pending_[i] = false;
      return i;
    }
  }
  return std::nullopt;
}

void GicV3::eoi(std::uint32_t intid) {
  (void)intid;
}

bool GicV3::save_state(std::ostream& out) const {
  return snapshot_io::write_array(out, pending_) &&
         snapshot_io::write_array(out, enabled_) &&
         snapshot_io::write(out, gicd_ctlr_) &&
         snapshot_io::write(out, gicr_waker_);
}

bool GicV3::load_state(std::istream& in) {
  return snapshot_io::read_array(in, pending_) &&
         snapshot_io::read_array(in, enabled_) &&
         snapshot_io::read(in, gicd_ctlr_) &&
         snapshot_io::read(in, gicr_waker_);
}

} // namespace aarchvm
