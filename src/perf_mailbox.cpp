#include "aarchvm/perf_mailbox.hpp"

#include "aarchvm/snapshot_io.hpp"

namespace aarchvm {

namespace {

constexpr std::uint64_t kRegId = 0x00;
constexpr std::uint64_t kRegCtrl = 0x08;
constexpr std::uint64_t kRegCaseId = 0x10;
constexpr std::uint64_t kRegArg0 = 0x18;
constexpr std::uint64_t kRegArg1 = 0x20;
constexpr std::uint64_t kRegStatus = 0x28;
constexpr std::uint64_t kRegHostNs = 0x30;
constexpr std::uint64_t kRegSteps = 0x38;
constexpr std::uint64_t kRegTlbHits = 0x40;
constexpr std::uint64_t kRegTlbMisses = 0x48;
constexpr std::uint64_t kRegPageWalks = 0x50;
constexpr std::uint64_t kRegBusReads = 0x58;
constexpr std::uint64_t kRegBusWrites = 0x60;

constexpr std::uint64_t kCmdBegin = 1;
constexpr std::uint64_t kCmdEnd = 2;
constexpr std::uint64_t kCmdExit = 3;
constexpr std::uint64_t kCmdFlushTlb = 4;

std::uint64_t extract_part(std::uint64_t value, std::uint64_t offset, std::size_t size) {
  const std::uint32_t shift = static_cast<std::uint32_t>((offset & 0x7u) * 8u);
  if (size == 1u) {
    return (value >> shift) & 0xFFu;
  }
  if (size == 2u) {
    return (value >> shift) & 0xFFFFu;
  }
  if (size == 4u) {
    return (value >> shift) & 0xFFFFFFFFu;
  }
  return value;
}

std::uint64_t merge_part(std::uint64_t cur, std::uint64_t value, std::uint64_t offset, std::size_t size) {
  const std::uint32_t shift = static_cast<std::uint32_t>((offset & 0x7u) * 8u);
  const std::uint64_t mask = (size == 1u) ? 0xFFull : ((size == 2u) ? 0xFFFFull : 0xFFFFFFFFull);
  return (cur & ~(mask << shift)) | ((value & mask) << shift);
}

} // namespace

PerfMailbox::PerfMailbox(Callbacks callbacks) : callbacks_(std::move(callbacks)) {}

void PerfMailbox::reset_state() {
  case_id_ = 0;
  arg0_ = 0;
  arg1_ = 0;
  last_status_ = 0;
  last_result_ = {};
}

std::uint64_t PerfMailbox::read(std::uint64_t offset, std::size_t size) {
  if (size != 1u && size != 2u && size != 4u && size != 8u) {
    return 0;
  }
  const std::uint64_t value = read_reg(offset & ~0x7ull);
  return extract_part(value, offset, size);
}

void PerfMailbox::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  if (size != 1u && size != 2u && size != 4u && size != 8u) {
    return;
  }
  const std::uint64_t aligned = offset & ~0x7ull;
  if (size == 8u) {
    write_reg(aligned, value);
    return;
  }
  const std::uint64_t merged = merge_part(read_reg(aligned), value, offset, size);
  write_reg(aligned, merged);
}

std::uint64_t PerfMailbox::read_reg(std::uint64_t offset) const {
  switch (offset) {
  case kRegId:
    return kMagic;
  case kRegCaseId:
    return case_id_;
  case kRegArg0:
    return arg0_;
  case kRegArg1:
    return arg1_;
  case kRegStatus:
    return last_status_;
  case kRegHostNs:
    return last_result_.host_ns;
  case kRegSteps:
    return last_result_.delta.steps;
  case kRegTlbHits:
    return last_result_.delta.tlb_hits;
  case kRegTlbMisses:
    return last_result_.delta.tlb_misses;
  case kRegPageWalks:
    return last_result_.delta.page_walks;
  case kRegBusReads:
    return last_result_.delta.bus_reads;
  case kRegBusWrites:
    return last_result_.delta.bus_writes;
  default:
    return 0;
  }
}

void PerfMailbox::write_reg(std::uint64_t offset, std::uint64_t value) {
  switch (offset) {
  case kRegCaseId:
    case_id_ = value;
    return;
  case kRegArg0:
    arg0_ = value;
    return;
  case kRegArg1:
    arg1_ = value;
    return;
  case kRegCtrl:
    switch (value) {
    case kCmdBegin:
      last_status_ = 1;
      if (callbacks_.begin) {
        callbacks_.begin(case_id_, arg0_, arg1_);
      }
      return;
    case kCmdEnd:
      if (callbacks_.end) {
        publish_result(callbacks_.end(case_id_, arg0_, arg1_));
      }
      last_status_ = 2;
      return;
    case kCmdExit:
      last_status_ = 3;
      if (callbacks_.request_exit) {
        callbacks_.request_exit();
      }
      return;
    case kCmdFlushTlb:
      last_status_ = 4;
      if (callbacks_.flush_tlb) {
        callbacks_.flush_tlb();
      }
      return;
    default:
      last_status_ = 0xdead0000ull | (value & 0xFFFFu);
      return;
    }
  default:
    return;
  }
}

void PerfMailbox::publish_result(const PerfResult& result) {
  last_result_ = result;
}

bool PerfMailbox::save_state(std::ostream& out) const {
  return snapshot_io::write(out, case_id_) &&
         snapshot_io::write(out, arg0_) &&
         snapshot_io::write(out, arg1_) &&
         snapshot_io::write(out, last_status_) &&
         snapshot_io::write(out, last_result_);
}

bool PerfMailbox::load_state(std::istream& in) {
  return snapshot_io::read(in, case_id_) &&
         snapshot_io::read(in, arg0_) &&
         snapshot_io::read(in, arg1_) &&
         snapshot_io::read(in, last_status_) &&
         snapshot_io::read(in, last_result_);
}

} // namespace aarchvm
