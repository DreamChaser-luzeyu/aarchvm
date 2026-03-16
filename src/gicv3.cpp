#include "aarchvm/gicv3.hpp"

#include "aarchvm/snapshot_io.hpp"

#include <algorithm>
#include <array>
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

void trace_gic(const char* tag, std::size_t cpu, std::uint32_t intid, bool value) {
  if (!gic_trace_enabled()) {
    return;
  }
  if (intid < 32u || intid == 33u || intid == 34u) {
    std::cerr << std::dec << tag << " cpu=" << cpu << " intid=" << intid << " val=" << (value ? 1 : 0) << '\n';
  }
}

std::uint32_t gicr_affinity_value(std::uint64_t mpidr) {
  return (static_cast<std::uint32_t>((mpidr >> 32) & 0xFFu) << 24u) |
         (static_cast<std::uint32_t>((mpidr >> 16) & 0xFFu) << 16u) |
         (static_cast<std::uint32_t>((mpidr >> 8) & 0xFFu) << 8u) |
         static_cast<std::uint32_t>(mpidr & 0xFFu);
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
constexpr std::uint32_t kGicrSgiIgroupr0 = 0x10080u;
constexpr std::uint32_t kGicrSgiIsenabler0 = 0x10100u;
constexpr std::uint32_t kGicrSgiIcenabler0 = 0x10180u;
constexpr std::uint32_t kGicrSgiIspendr0 = 0x10200u;
constexpr std::uint32_t kGicrSgiIcpendr0 = 0x10280u;
constexpr std::uint32_t kGicrSgiIsactiver0 = 0x10300u;
constexpr std::uint32_t kGicrSgiIcactiver0 = 0x10380u;
constexpr std::uint32_t kGicrSgiIpriorityr = 0x10400u;
constexpr std::uint32_t kGicrSgiIcfgr0 = 0x10C00u;
constexpr std::uint32_t kGicrSgiIcfgr1 = 0x10C04u;

constexpr std::uint32_t kGicArchRevGicv3 = 0x3u << 4;
constexpr std::uint64_t kRedistBaseConst = 0x0A0000;

bool is_write1_bitmap_register(std::uint64_t offset) {
  return (offset >= kGicdIsenabler && offset < (kGicdIsenabler + 0x80u)) ||
         (offset >= kGicdIcenabler && offset < (kGicdIcenabler + 0x80u)) ||
         (offset >= kGicdIsactiver && offset < (kGicdIsactiver + 0x80u)) ||
         (offset >= kGicdIcactiver && offset < (kGicdIcactiver + 0x80u)) ||
         (offset >= kRedistBaseConst + kGicrSgiIsenabler0 && offset < (kRedistBaseConst + kGicrSgiIsenabler0 + 4u)) ||
         (offset >= kRedistBaseConst + kGicrSgiIcenabler0 && offset < (kRedistBaseConst + kGicrSgiIcenabler0 + 4u)) ||
         (offset >= kRedistBaseConst + kGicrSgiIspendr0 && offset < (kRedistBaseConst + kGicrSgiIspendr0 + 4u)) ||
         (offset >= kRedistBaseConst + kGicrSgiIcpendr0 && offset < (kRedistBaseConst + kGicrSgiIcpendr0 + 4u)) ||
         (offset >= kRedistBaseConst + kGicrSgiIsactiver0 && offset < (kRedistBaseConst + kGicrSgiIsactiver0 + 4u)) ||
         (offset >= kRedistBaseConst + kGicrSgiIcactiver0 && offset < (kRedistBaseConst + kGicrSgiIcactiver0 + 4u));
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
  set_cpu_count(1);
  spi_priorities_.fill(kDefaultPriority);
}

void GicV3::set_cpu_count(std::size_t cpu_count) {
  cpu_count_ = std::max<std::size_t>(1, cpu_count);
  locals_.assign(cpu_count_, LocalCpuState{});
  for (auto& local : locals_) {
    local.priorities.fill(kDefaultPriority);
  }
  ++state_epoch_;
}

void GicV3::set_cpu_affinity(std::size_t cpu_index, std::uint64_t mpidr) {
  local_cpu(cpu_index).mpidr = mpidr;
}

GicV3::LocalCpuState& GicV3::local_cpu(std::size_t cpu_index) {
  return locals_[std::min(cpu_index, locals_.size() - 1u)];
}

const GicV3::LocalCpuState& GicV3::local_cpu(std::size_t cpu_index) const {
  return locals_[std::min(cpu_index, locals_.size() - 1u)];
}

std::uint64_t GicV3::read(std::uint64_t offset, std::size_t size) {
  if (size != 1u && size != 2u && size != 4u && size != 8u) {
    return 0;
  }

  const auto read32 = [&](std::uint64_t off) {
    return (off < kDistSize) ? read_dist_reg32(off & ~0x3ull) : read_redist_reg32(off & ~0x3ull);
  };

  const std::uint32_t value32 = read32(offset);
  if (size == 8u) {
    const std::uint64_t lo = value32;
    const std::uint64_t hi = read32((offset & ~0x7ull) + 4u);
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
    return (n == 0u) ? 0u : get_spi_enable_range(n * 32u);
  }
  if (offset >= kGicdIcenabler && offset < (kGicdIcenabler + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIcenabler) / 4u);
    return (n == 0u) ? 0u : get_spi_enable_range(n * 32u);
  }
  if (offset >= kGicdIsactiver && offset < (kGicdIsactiver + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIsactiver) / 4u);
    return (n == 0u) ? 0u : get_spi_active_range(n * 32u);
  }
  if (offset >= kGicdIcactiver && offset < (kGicdIcactiver + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIcactiver) / 4u);
    return (n == 0u) ? 0u : get_spi_active_range(n * 32u);
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
  if (offset < kRedistBase) {
    return 0;
  }
  const std::uint64_t rel = offset - kRedistBase;
  const std::size_t frame = static_cast<std::size_t>(rel / kRedistFrameSize);
  if (frame >= locals_.size()) {
    return 0;
  }
  const std::uint64_t inner = rel % kRedistFrameSize;
  const auto& local = local_cpu(frame);

  if (inner == kGicrTyper) {
    const bool last = (frame + 1u) == locals_.size();
    const std::uint32_t proc_num = static_cast<std::uint32_t>(frame) << 8u;
    return (last ? (1u << 4) : 0u) | proc_num;
  }
  if (inner == (kGicrTyper + 4u)) {
    return gicr_affinity_value(local.mpidr);
  }
  if (inner == kGicrWaker) {
    return local.gicr_waker & ~0x4u;
  }
  if (inner == kGicrPidr2) {
    return kGicArchRevGicv3;
  }
  if (inner == kGicrSgiIgroupr0) {
    return local.gicr_igroupr0;
  }
  if (inner == kGicrSgiIsenabler0 || inner == kGicrSgiIcenabler0) {
    std::uint32_t value = 0;
    for (std::uint32_t intid = 0; intid < kLocalIntIds; ++intid) {
      if (local.enabled[intid]) {
        value |= (1u << intid);
      }
    }
    return value;
  }
  if (inner == kGicrSgiIspendr0 || inner == kGicrSgiIcpendr0) {
    std::uint32_t value = 0;
    for (std::uint32_t intid = 0; intid < kLocalIntIds; ++intid) {
      if (local.pending[intid]) {
        value |= (1u << intid);
      }
    }
    return value;
  }
  if (inner == kGicrSgiIsactiver0 || inner == kGicrSgiIcactiver0) {
    std::uint32_t value = 0;
    for (std::uint32_t intid = 0; intid < kLocalIntIds; ++intid) {
      if (local.active[intid]) {
        value |= (1u << intid);
      }
    }
    return value;
  }
  if (inner >= kGicrSgiIpriorityr && inner < (kGicrSgiIpriorityr + kLocalIntIds)) {
    std::uint32_t out = 0;
    const std::uint32_t first = static_cast<std::uint32_t>(inner - kGicrSgiIpriorityr);
    for (std::uint32_t i = 0; i < 4u; ++i) {
      const std::uint32_t intid = first + i;
      if (intid >= kLocalIntIds) {
        break;
      }
      out |= static_cast<std::uint32_t>(local.priorities[intid]) << (i * 8u);
    }
    return out;
  }
  if (inner == kGicrSgiIcfgr0) {
    return local.gicr_icfgr0;
  }
  if (inner == kGicrSgiIcfgr1) {
    return local.gicr_icfgr1;
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
      set_spi_enable_range(n * 32u, value, true);
    }
    return;
  }
  if (offset >= kGicdIcenabler && offset < (kGicdIcenabler + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIcenabler) / 4u);
    if (n != 0u) {
      set_spi_enable_range(n * 32u, value, false);
    }
    return;
  }
  if (offset >= kGicdIsactiver && offset < (kGicdIsactiver + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIsactiver) / 4u);
    if (n != 0u) {
      set_spi_active_range(n * 32u, value, true);
    }
    return;
  }
  if (offset >= kGicdIcactiver && offset < (kGicdIcactiver + 0x80u)) {
    const std::uint32_t n = static_cast<std::uint32_t>((offset - kGicdIcactiver) / 4u);
    if (n != 0u) {
      set_spi_active_range(n * 32u, value, false);
    }
    return;
  }
  if (offset >= kGicdIpriorityr && offset < (kGicdIpriorityr + kNumIntIds)) {
    const std::uint32_t first = static_cast<std::uint32_t>(offset - kGicdIpriorityr);
    set_priority_range(first, value, 0u, 4u, 0u, false);
  }
}

void GicV3::write_redist_reg32(std::uint64_t offset, std::uint32_t value) {
  if (offset < kRedistBase) {
    return;
  }
  const std::uint64_t rel = offset - kRedistBase;
  const std::size_t frame = static_cast<std::size_t>(rel / kRedistFrameSize);
  if (frame >= locals_.size()) {
    return;
  }
  const std::uint64_t inner = rel % kRedistFrameSize;
  auto& local = local_cpu(frame);

  if (inner == kGicrWaker) {
    local.gicr_waker = value & 0x2u;
    return;
  }
  if (inner == kGicrSgiIgroupr0) {
    local.gicr_igroupr0 = value;
    return;
  }
  if (inner == kGicrSgiIsenabler0) {
    for (std::uint32_t intid = 0; intid < kLocalIntIds; ++intid) {
      if ((value & (1u << intid)) != 0u) {
        set_local_enabled_bit(frame, intid, true);
      }
    }
    return;
  }
  if (inner == kGicrSgiIcenabler0) {
    for (std::uint32_t intid = 0; intid < kLocalIntIds; ++intid) {
      if ((value & (1u << intid)) != 0u) {
        set_local_enabled_bit(frame, intid, false);
      }
    }
    return;
  }
  if (inner == kGicrSgiIspendr0) {
    for (std::uint32_t intid = 0; intid < kLocalIntIds; ++intid) {
      if ((value & (1u << intid)) != 0u) {
        set_local_pending_bit(frame, intid, true);
      }
    }
    return;
  }
  if (inner == kGicrSgiIcpendr0) {
    for (std::uint32_t intid = 0; intid < kLocalIntIds; ++intid) {
      if ((value & (1u << intid)) != 0u) {
        set_local_pending_bit(frame, intid, false);
      }
    }
    return;
  }
  if (inner == kGicrSgiIsactiver0) {
    for (std::uint32_t intid = 0; intid < kLocalIntIds; ++intid) {
      if ((value & (1u << intid)) != 0u) {
        set_local_active_bit(frame, intid, true);
      }
    }
    return;
  }
  if (inner == kGicrSgiIcactiver0) {
    for (std::uint32_t intid = 0; intid < kLocalIntIds; ++intid) {
      if ((value & (1u << intid)) != 0u) {
        set_local_active_bit(frame, intid, false);
      }
    }
    return;
  }
  if (inner >= kGicrSgiIpriorityr && inner < (kGicrSgiIpriorityr + kLocalIntIds)) {
    set_priority_range(static_cast<std::uint32_t>(inner - kGicrSgiIpriorityr), value, 0u, 4u, frame, true);
    return;
  }
  if (inner == kGicrSgiIcfgr0) {
    local.gicr_icfgr0 = value;
    return;
  }
  if (inner == kGicrSgiIcfgr1) {
    local.gicr_icfgr1 = value;
    return;
  }
}

void GicV3::set_spi_enable_range(std::uint32_t first_intid, std::uint32_t bits, bool enable) {
  for (std::uint32_t bit = 0; bit < 32u; ++bit) {
    if ((bits & (1u << bit)) == 0u) {
      continue;
    }
    const std::uint32_t intid = first_intid + bit;
    if (intid >= kNumIntIds || intid < kFirstSpiIntId) {
      continue;
    }
    set_spi_enabled_bit(intid, enable);
  }
}

std::uint32_t GicV3::get_spi_enable_range(std::uint32_t first_intid) const {
  std::uint32_t value = 0;
  for (std::uint32_t bit = 0; bit < 32u; ++bit) {
    const std::uint32_t intid = first_intid + bit;
    if (intid >= kNumIntIds || intid < kFirstSpiIntId) {
      continue;
    }
    if (spi_enabled_[spi_index(intid)]) {
      value |= (1u << bit);
    }
  }
  return value;
}

void GicV3::set_spi_active_range(std::uint32_t first_intid, std::uint32_t bits, bool value) {
  for (std::uint32_t bit = 0; bit < 32u; ++bit) {
    if ((bits & (1u << bit)) == 0u) {
      continue;
    }
    const std::uint32_t intid = first_intid + bit;
    if (intid >= kNumIntIds || intid < kFirstSpiIntId) {
      continue;
    }
    set_spi_active_bit(intid, value);
  }
}

std::uint32_t GicV3::get_spi_active_range(std::uint32_t first_intid) const {
  std::uint32_t value = 0;
  for (std::uint32_t bit = 0; bit < 32u; ++bit) {
    const std::uint32_t intid = first_intid + bit;
    if (intid >= kNumIntIds || intid < kFirstSpiIntId) {
      continue;
    }
    if (spi_active_[spi_index(intid)]) {
      value |= (1u << bit);
    }
  }
  return value;
}

std::uint32_t GicV3::get_priority_range(std::uint32_t first_intid) const {
  std::uint32_t value = 0;
  for (std::uint32_t i = 0; i < 4u; ++i) {
    const std::uint32_t intid = first_intid + i;
    std::uint8_t prio = kDefaultPriority;
    if (intid < kLocalIntIds) {
      prio = locals_[0].priorities[intid];
    } else if (intid < kNumIntIds) {
      prio = spi_priorities_[spi_index(intid)];
    }
    value |= static_cast<std::uint32_t>(prio) << (i * 8u);
  }
  return value;
}

void GicV3::set_priority_range(std::uint32_t first_intid,
                               std::uint32_t value,
                               std::uint32_t start_byte,
                               std::uint32_t count,
                               std::size_t cpu_index,
                               bool local) {
  for (std::uint32_t i = start_byte; i < count; ++i) {
    const std::uint32_t intid = first_intid + i;
    const std::uint8_t prio = static_cast<std::uint8_t>((value >> (i * 8u)) & 0xFFu);
    if (local) {
      if (intid < kLocalIntIds) {
        local_cpu(cpu_index).priorities[intid] = prio;
      }
    } else if (intid >= kFirstSpiIntId && intid < kNumIntIds) {
      spi_priorities_[spi_index(intid)] = prio;
    }
  }
  ++state_epoch_;
}

void GicV3::set_local_pending_bit(std::size_t cpu_index, std::uint32_t intid, bool value) {
  auto& local = local_cpu(cpu_index);
  if (intid >= kLocalIntIds || local.pending[intid] == value) {
    return;
  }
  local.pending[intid] = value;
  ++state_epoch_;
  trace_gic("GIC-PEND-L", cpu_index, intid, value);
}

void GicV3::set_local_enabled_bit(std::size_t cpu_index, std::uint32_t intid, bool value) {
  auto& local = local_cpu(cpu_index);
  if (intid >= kLocalIntIds || local.enabled[intid] == value) {
    return;
  }
  local.enabled[intid] = value;
  ++state_epoch_;
}

void GicV3::set_local_active_bit(std::size_t cpu_index, std::uint32_t intid, bool value) {
  auto& local = local_cpu(cpu_index);
  if (intid >= kLocalIntIds || local.active[intid] == value) {
    return;
  }
  local.active[intid] = value;
  ++state_epoch_;
}

void GicV3::set_spi_pending_bit(std::uint32_t intid, bool value) {
  if (intid < kFirstSpiIntId || intid >= kNumIntIds) {
    return;
  }
  auto& slot = spi_pending_[spi_index(intid)];
  if (slot == value) {
    return;
  }
  slot = value;
  ++state_epoch_;
  trace_gic("GIC-PEND-S", 0, intid, value);
}

void GicV3::set_spi_enabled_bit(std::uint32_t intid, bool value) {
  if (intid < kFirstSpiIntId || intid >= kNumIntIds) {
    return;
  }
  auto& slot = spi_enabled_[spi_index(intid)];
  if (slot == value) {
    return;
  }
  slot = value;
  ++state_epoch_;
}

void GicV3::set_spi_active_bit(std::uint32_t intid, bool value) {
  if (intid < kFirstSpiIntId || intid >= kNumIntIds) {
    return;
  }
  auto& slot = spi_active_[spi_index(intid)];
  if (slot == value) {
    return;
  }
  slot = value;
  ++state_epoch_;
}

void GicV3::set_pending(std::uint32_t intid) {
  if (is_local_intid(intid)) {
    set_pending(0, intid);
    return;
  }
  set_spi_pending_bit(intid, true);
}

void GicV3::set_pending(std::size_t cpu_index, std::uint32_t intid) {
  if (is_local_intid(intid)) {
    set_local_pending_bit(cpu_index, intid, true);
  } else {
    set_spi_pending_bit(intid, true);
  }
}

void GicV3::set_level(std::uint32_t intid, bool asserted) {
  ++perf_counters_.set_level_calls;
  if (is_local_intid(intid)) {
    set_level(0, intid, asserted);
    return;
  }
  if (intid >= kNumIntIds || spi_line_level_[spi_index(intid)] == asserted) {
    return;
  }
  spi_line_level_[spi_index(intid)] = asserted;
  ++state_epoch_;
  if (asserted && !spi_active_[spi_index(intid)]) {
    set_spi_pending_bit(intid, true);
  }
  trace_gic("GIC-LVL-S", 0, intid, asserted);
}

void GicV3::set_level(std::size_t cpu_index, std::uint32_t intid, bool asserted) {
  ++perf_counters_.set_level_calls;
  if (!is_local_intid(intid)) {
    set_level(intid, asserted);
    return;
  }
  auto& local = local_cpu(cpu_index);
  if (local.line_level[intid] == asserted) {
    return;
  }
  local.line_level[intid] = asserted;
  ++state_epoch_;
  if (asserted && !local.active[intid]) {
    set_local_pending_bit(cpu_index, intid, true);
  }
  trace_gic("GIC-LVL-L", cpu_index, intid, asserted);
}

bool GicV3::match_affinity(std::size_t cpu_index,
                           std::uint8_t aff3,
                           std::uint8_t aff2,
                           std::uint8_t aff1) const {
  const std::uint64_t mpidr = local_cpu(cpu_index).mpidr;
  return (((mpidr >> 32) & 0xFFu) == aff3) &&
         (((mpidr >> 16) & 0xFFu) == aff2) &&
         (((mpidr >> 8) & 0xFFu) == aff1);
}

void GicV3::send_sgi(std::size_t source_cpu, std::uint64_t value) {
  const std::uint32_t intid = static_cast<std::uint32_t>((value >> 24) & 0xFu);
  const bool irm = ((value >> 40) & 0x1u) != 0u;
  const std::uint16_t target_list = static_cast<std::uint16_t>(value & 0xFFFFu);
  const std::uint8_t aff1 = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
  const std::uint8_t aff2 = static_cast<std::uint8_t>((value >> 32) & 0xFFu);
  const std::uint8_t aff3 = static_cast<std::uint8_t>((value >> 48) & 0xFFu);
  if (intid >= 16u) {
    return;
  }

  for (std::size_t cpu = 0; cpu < locals_.size(); ++cpu) {
    if (irm) {
      if (cpu == source_cpu) {
        continue;
      }
      set_local_pending_bit(cpu, intid, true);
      continue;
    }
    if (!match_affinity(cpu, aff3, aff2, aff1)) {
      continue;
    }
    const std::uint32_t aff0 = static_cast<std::uint32_t>(local_cpu(cpu).mpidr & 0xFFu);
    if (aff0 < 16u && ((target_list >> aff0) & 0x1u) != 0u) {
      set_local_pending_bit(cpu, intid, true);
    }
  }
}

bool GicV3::local_candidate(std::size_t cpu_index, std::uint32_t intid, std::uint8_t pmr) const {
  const auto& local = local_cpu(cpu_index);
  return local.pending[intid] && local.enabled[intid] && local.priorities[intid] < pmr;
}

bool GicV3::spi_candidate(std::uint32_t intid, std::uint8_t pmr) const {
  return spi_pending_[spi_index(intid)] && spi_enabled_[spi_index(intid)] && spi_priorities_[spi_index(intid)] < pmr;
}

bool GicV3::has_pending(std::size_t cpu_index) const {
  return has_pending(cpu_index, 0xFFu);
}

bool GicV3::has_pending(std::size_t cpu_index, std::uint8_t pmr) const {
  ++perf_counters_.has_pending_calls;
  for (std::uint32_t intid = 0; intid < kLocalIntIds; ++intid) {
    if (local_candidate(cpu_index, intid, pmr)) {
      return true;
    }
  }
  for (std::uint32_t intid = kFirstSpiIntId; intid < kNumIntIds; ++intid) {
    if (spi_candidate(intid, pmr)) {
      return true;
    }
  }
  return false;
}

bool GicV3::acknowledge(std::size_t cpu_index, std::uint32_t& intid) {
  return acknowledge(cpu_index, 0xFFu, intid);
}

bool GicV3::acknowledge(std::size_t cpu_index, std::uint8_t pmr, std::uint32_t& intid) {
  ++perf_counters_.acknowledge_calls;
  for (std::uint32_t local_intid = 0; local_intid < kLocalIntIds; ++local_intid) {
    if (!local_candidate(cpu_index, local_intid, pmr)) {
      continue;
    }
    intid = local_intid;
    set_local_pending_bit(cpu_index, local_intid, false);
    set_local_active_bit(cpu_index, local_intid, true);
    return true;
  }
  for (std::uint32_t global_intid = kFirstSpiIntId; global_intid < kNumIntIds; ++global_intid) {
    if (!spi_candidate(global_intid, pmr)) {
      continue;
    }
    intid = global_intid;
    set_spi_pending_bit(global_intid, false);
    set_spi_active_bit(global_intid, true);
    return true;
  }
  intid = 1023u;
  return false;
}

void GicV3::eoi(std::size_t cpu_index, std::uint32_t intid) {
  if (is_local_intid(intid)) {
    set_local_active_bit(cpu_index, intid, false);
    if (local_cpu(cpu_index).line_level[intid]) {
      set_local_pending_bit(cpu_index, intid, true);
    }
    return;
  }
  if (intid >= kNumIntIds) {
    return;
  }
  set_spi_active_bit(intid, false);
  if (spi_line_level_[spi_index(intid)]) {
    set_spi_pending_bit(intid, true);
  }
}

bool GicV3::pending(std::uint32_t intid) const {
  if (intid < kLocalIntIds) {
    return local_cpu(0).pending[intid];
  }
  if (intid >= kNumIntIds) {
    return false;
  }
  return spi_pending_[spi_index(intid)];
}

bool GicV3::enabled(std::uint32_t intid) const {
  if (intid < kLocalIntIds) {
    return local_cpu(0).enabled[intid];
  }
  if (intid >= kNumIntIds) {
    return false;
  }
  return spi_enabled_[spi_index(intid)];
}

std::uint8_t GicV3::priority(std::size_t cpu_index, std::uint32_t intid) const {
  if (intid < kLocalIntIds) {
    return local_cpu(cpu_index).priorities[intid];
  }
  if (intid >= kNumIntIds) {
    return kDefaultPriority;
  }
  return spi_priorities_[spi_index(intid)];
}

bool GicV3::save_state(std::ostream& out) const {
  const std::uint32_t snapshot_cpu_count = static_cast<std::uint32_t>(locals_.size());
  if (!snapshot_io::write(out, snapshot_cpu_count) ||
      !snapshot_io::write_array(out, spi_pending_) ||
      !snapshot_io::write_array(out, spi_enabled_) ||
      !snapshot_io::write_array(out, spi_active_) ||
      !snapshot_io::write_array(out, spi_line_level_) ||
      !snapshot_io::write_array(out, spi_priorities_) ||
      !snapshot_io::write(out, gicd_ctlr_)) {
    return false;
  }
  for (const auto& local : locals_) {
    if (!snapshot_io::write_array(out, local.pending) ||
        !snapshot_io::write_array(out, local.enabled) ||
        !snapshot_io::write_array(out, local.active) ||
        !snapshot_io::write_array(out, local.line_level) ||
        !snapshot_io::write_array(out, local.priorities) ||
        !snapshot_io::write(out, local.gicr_waker) ||
        !snapshot_io::write(out, local.gicr_igroupr0) ||
        !snapshot_io::write(out, local.gicr_icfgr0) ||
        !snapshot_io::write(out, local.gicr_icfgr1) ||
        !snapshot_io::write(out, local.mpidr)) {
      return false;
    }
  }
  return true;
}

bool GicV3::load_state(std::istream& in, std::uint32_t version) {
  spi_pending_.fill(false);
  spi_enabled_.fill(false);
  spi_active_.fill(false);
  spi_line_level_.fill(false);
  spi_priorities_.fill(kDefaultPriority);
  set_cpu_count(1);

  if (version >= 5) {
    std::uint32_t snapshot_cpu_count = 0;
    if (!snapshot_io::read(in, snapshot_cpu_count) || snapshot_cpu_count == 0u) {
      return false;
    }
    set_cpu_count(snapshot_cpu_count);
    if (!snapshot_io::read_array(in, spi_pending_) ||
        !snapshot_io::read_array(in, spi_enabled_) ||
        !snapshot_io::read_array(in, spi_active_) ||
        !snapshot_io::read_array(in, spi_line_level_) ||
        !snapshot_io::read_array(in, spi_priorities_) ||
        !snapshot_io::read(in, gicd_ctlr_)) {
      return false;
    }
    for (auto& local : locals_) {
      if (!snapshot_io::read_array(in, local.pending) ||
          !snapshot_io::read_array(in, local.enabled) ||
          !snapshot_io::read_array(in, local.active) ||
          !snapshot_io::read_array(in, local.line_level) ||
          !snapshot_io::read_array(in, local.priorities) ||
          !snapshot_io::read(in, local.gicr_waker) ||
          !snapshot_io::read(in, local.gicr_igroupr0) ||
          !snapshot_io::read(in, local.gicr_icfgr0) ||
          !snapshot_io::read(in, local.gicr_icfgr1) ||
          !snapshot_io::read(in, local.mpidr)) {
        return false;
      }
    }
  } else {
    std::array<bool, kNumIntIds> pending{};
    std::array<bool, kNumIntIds> enabled{};
    std::array<bool, kNumIntIds> active{};
    std::array<bool, kNumIntIds> line_level{};
    std::array<std::uint8_t, kNumIntIds> priorities{};
    std::uint32_t gicr_waker = 0;
    std::uint32_t gicr_igroupr0 = 0xFFFFFFFFu;
    std::uint32_t gicr_icfgr0 = 0xAAAAAAAAu;
    std::uint32_t gicr_icfgr1 = 0;
    priorities.fill(kDefaultPriority);
    if (!snapshot_io::read_array(in, pending) ||
        !snapshot_io::read_array(in, enabled)) {
      return false;
    }
    if (version >= 3) {
      if (!snapshot_io::read_array(in, active)) {
        return false;
      }
      if (version >= 4) {
        if (!snapshot_io::read_array(in, line_level)) {
          return false;
        }
      }
      if (!snapshot_io::read_array(in, priorities)) {
        return false;
      }
    }
    if (!snapshot_io::read(in, gicd_ctlr_) ||
        !snapshot_io::read(in, gicr_waker)) {
      return false;
    }
    if (version >= 3) {
      if (!snapshot_io::read(in, gicr_igroupr0) ||
          !snapshot_io::read(in, gicr_icfgr0) ||
          !snapshot_io::read(in, gicr_icfgr1)) {
        return false;
      }
    }
    for (std::uint32_t intid = 0; intid < kNumIntIds; ++intid) {
      if (intid < kLocalIntIds) {
        locals_[0].pending[intid] = pending[intid];
        locals_[0].enabled[intid] = enabled[intid];
        locals_[0].active[intid] = active[intid];
        locals_[0].line_level[intid] = line_level[intid];
        locals_[0].priorities[intid] = priorities[intid];
      } else {
        spi_pending_[spi_index(intid)] = pending[intid];
        spi_enabled_[spi_index(intid)] = enabled[intid];
        spi_active_[spi_index(intid)] = active[intid];
        spi_line_level_[spi_index(intid)] = line_level[intid];
        spi_priorities_[spi_index(intid)] = priorities[intid];
      }
    }
    locals_[0].gicr_waker = gicr_waker;
    locals_[0].gicr_igroupr0 = gicr_igroupr0;
    locals_[0].gicr_icfgr0 = gicr_icfgr0;
    locals_[0].gicr_icfgr1 = gicr_icfgr1;
  }

  ++state_epoch_;
  return true;
}

} // namespace aarchvm
