#include "aarchvm/pl050_kmi.hpp"

#include "aarchvm/snapshot_io.hpp"

namespace aarchvm {

namespace {
constexpr std::uint8_t kCrRxIntrEn = 1u << 4;
constexpr std::uint8_t kCrTxIntrEn = 1u << 3;
constexpr std::uint8_t kCrEn = 1u << 2;

constexpr std::uint8_t kStatTxEmpty = 1u << 6;
constexpr std::uint8_t kStatRxFull = 1u << 4;
constexpr std::uint8_t kStatIc = 1u << 1;
constexpr std::uint8_t kStatId = 1u << 0;

constexpr std::uint8_t kIrTxIntr = 1u << 1;
constexpr std::uint8_t kIrRxIntr = 1u << 0;

constexpr std::uint8_t kPs2Ack = 0xFAu;
constexpr std::uint8_t kPs2BatOk = 0xAAu;
constexpr std::uint8_t kPs2Echo = 0xEEu;
constexpr std::uint8_t kPs2Resend = 0xFEu;
constexpr std::uint8_t kPs2KeyboardId0 = 0xABu;
constexpr std::uint8_t kPs2KeyboardId1 = 0x83u;

constexpr std::size_t kMaxRxFifo = 1024u;
}

std::uint64_t Pl050Kmi::read(std::uint64_t offset, std::size_t size) {
  if (size != 1 && size != 2 && size != 4 && size != 8) {
    return 0;
  }

  switch (offset) {
    case 0x00:
      return cr_;
    case 0x04: {
      std::uint8_t stat = static_cast<std::uint8_t>(kStatTxEmpty | kStatIc | kStatId);
      if (!rx_fifo_.empty()) {
        stat = static_cast<std::uint8_t>(stat | kStatRxFull);
      }
      return stat;
    }
    case 0x08: {
      if (rx_fifo_.empty()) {
        return 0;
      }
      const std::uint8_t byte = rx_fifo_.front();
      rx_fifo_.pop_front();
      update_irq_state();
      return byte;
    }
    case 0x0c:
      return clkdiv_;
    case 0x10: {
      std::uint8_t ir = 0;
      if ((cr_ & kCrTxIntrEn) != 0u) {
        ir = static_cast<std::uint8_t>(ir | kIrTxIntr);
      }
      if (irq_pending()) {
        ir = static_cast<std::uint8_t>(ir | kIrRxIntr);
      }
      return ir;
    }
    case 0xFE0:
      return 0x50u;
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

void Pl050Kmi::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  if (size != 1 && size != 2 && size != 4 && size != 8) {
    return;
  }

  const std::uint8_t byte = static_cast<std::uint8_t>(value & 0xFFu);
  switch (offset) {
    case 0x00:
      cr_ = byte;
      update_irq_state();
      return;
    case 0x08:
      last_tx_byte_ = byte;
      process_host_byte(byte);
      return;
    case 0x0c:
      clkdiv_ = static_cast<std::uint8_t>(byte & 0x0Fu);
      return;
    default:
      return;
  }
}

void Pl050Kmi::inject_rx(std::uint8_t byte) {
  if (!scanning_enabled_) {
    return;
  }
  if (rx_fifo_.size() < kMaxRxFifo) {
    rx_fifo_.push_back(byte);
  }
  update_irq_state();
}

bool Pl050Kmi::irq_pending() const {
  return (cr_ & (kCrEn | kCrRxIntrEn)) == (kCrEn | kCrRxIntrEn) && !rx_fifo_.empty();
}

void Pl050Kmi::reset() {
  rx_fifo_.clear();
  cr_ = 0;
  clkdiv_ = 0;
  last_tx_byte_ = 0;
  reset_defaults();
}

bool Pl050Kmi::save_state(std::ostream& out) const {
  std::vector<std::uint8_t> fifo(rx_fifo_.begin(), rx_fifo_.end());
  const std::uint8_t pending = static_cast<std::uint8_t>(pending_command_);
  return snapshot_io::write_vector(out, fifo) &&
         snapshot_io::write(out, cr_) &&
         snapshot_io::write(out, clkdiv_) &&
         snapshot_io::write(out, last_tx_byte_) &&
         snapshot_io::write(out, scan_set_) &&
         snapshot_io::write_bool(out, scanning_enabled_) &&
         snapshot_io::write(out, pending);
}

bool Pl050Kmi::load_state(std::istream& in) {
  std::vector<std::uint8_t> fifo;
  std::uint8_t pending = 0;
  if (!snapshot_io::read_vector(in, fifo) || fifo.size() > kMaxRxFifo ||
      !snapshot_io::read(in, cr_) ||
      !snapshot_io::read(in, clkdiv_) ||
      !snapshot_io::read(in, last_tx_byte_) ||
      !snapshot_io::read(in, scan_set_) ||
      !snapshot_io::read_bool(in, scanning_enabled_) ||
      !snapshot_io::read(in, pending)) {
    return false;
  }
  rx_fifo_.clear();
  for (std::uint8_t byte : fifo) {
    rx_fifo_.push_back(byte);
  }
  pending_command_ = static_cast<PendingCommand>(pending);
  update_irq_state();
  return true;
}

void Pl050Kmi::update_irq_state() {
  if ((cr_ & kCrEn) == 0u) {
    rx_fifo_.clear();
  }
  if (state_change_observer_) {
    state_change_observer_();
  }
}

void Pl050Kmi::queue_response(std::uint8_t byte) {
  if (rx_fifo_.size() < kMaxRxFifo) {
    rx_fifo_.push_back(byte);
  }
  update_irq_state();
}

void Pl050Kmi::reset_defaults() {
  scan_set_ = 2;
  scanning_enabled_ = true;
  pending_command_ = PendingCommand::None;
}

void Pl050Kmi::process_host_byte(std::uint8_t byte) {
  if ((cr_ & kCrEn) == 0u) {
    return;
  }

  switch (pending_command_) {
    case PendingCommand::SetLeds:
    case PendingCommand::SetTypematic:
    case PendingCommand::SetAllLeds:
      pending_command_ = PendingCommand::None;
      queue_response(kPs2Ack);
      return;
    case PendingCommand::SetScanSet:
      pending_command_ = PendingCommand::None;
      queue_response(kPs2Ack);
      if (byte == 0x00u) {
        queue_response(scan_set_);
      } else if (byte >= 1u && byte <= 3u) {
        scan_set_ = byte;
      }
      return;
    case PendingCommand::None:
      break;
  }

  switch (byte) {
    case 0xEDu:
      pending_command_ = PendingCommand::SetLeds;
      queue_response(kPs2Ack);
      return;
    case 0xEEu:
      queue_response(kPs2Echo);
      return;
    case 0xF0u:
      pending_command_ = PendingCommand::SetScanSet;
      queue_response(kPs2Ack);
      return;
    case 0xF2u:
      queue_response(kPs2Ack);
      queue_response(kPs2KeyboardId0);
      queue_response(kPs2KeyboardId1);
      return;
    case 0xF3u:
      pending_command_ = PendingCommand::SetTypematic;
      queue_response(kPs2Ack);
      return;
    case 0xF4u:
      scanning_enabled_ = true;
      queue_response(kPs2Ack);
      return;
    case 0xF5u:
      reset_defaults();
      scanning_enabled_ = false;
      queue_response(kPs2Ack);
      return;
    case 0xF6u:
      reset_defaults();
      queue_response(kPs2Ack);
      return;
    case 0xF8u:
    case 0xFAu:
    case 0xEAu:
    case 0xE8u:
      pending_command_ = (byte == 0xEAu || byte == 0xE8u) ? PendingCommand::SetAllLeds : PendingCommand::None;
      queue_response(kPs2Ack);
      return;
    case 0xFEu:
      queue_response(last_tx_byte_ == 0 ? kPs2Resend : last_tx_byte_);
      return;
    case 0xFFu:
      reset_defaults();
      queue_response(kPs2Ack);
      queue_response(kPs2BatOk);
      return;
    default:
      queue_response(kPs2Ack);
      return;
  }
}

} // namespace aarchvm
