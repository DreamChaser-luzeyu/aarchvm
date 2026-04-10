#include "aarchvm/virtio_net_mmio.hpp"

#include "aarchvm/bus.hpp"
#include "aarchvm/snapshot_io.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace aarchvm {

namespace {

constexpr std::uint64_t kRegMagicValue = 0x000;
constexpr std::uint64_t kRegVersion = 0x004;
constexpr std::uint64_t kRegDeviceId = 0x008;
constexpr std::uint64_t kRegVendorId = 0x00c;
constexpr std::uint64_t kRegDeviceFeatures = 0x010;
constexpr std::uint64_t kRegDeviceFeaturesSel = 0x014;
constexpr std::uint64_t kRegDriverFeatures = 0x020;
constexpr std::uint64_t kRegDriverFeaturesSel = 0x024;
constexpr std::uint64_t kRegGuestPageSize = 0x028;
constexpr std::uint64_t kRegQueueSel = 0x030;
constexpr std::uint64_t kRegQueueNumMax = 0x034;
constexpr std::uint64_t kRegQueueNum = 0x038;
constexpr std::uint64_t kRegQueueReady = 0x044;
constexpr std::uint64_t kRegQueueNotify = 0x050;
constexpr std::uint64_t kRegInterruptStatus = 0x060;
constexpr std::uint64_t kRegInterruptAck = 0x064;
constexpr std::uint64_t kRegStatus = 0x070;
constexpr std::uint64_t kRegQueueDescLow = 0x080;
constexpr std::uint64_t kRegQueueDescHigh = 0x084;
constexpr std::uint64_t kRegQueueAvailLow = 0x090;
constexpr std::uint64_t kRegQueueAvailHigh = 0x094;
constexpr std::uint64_t kRegQueueUsedLow = 0x0a0;
constexpr std::uint64_t kRegQueueUsedHigh = 0x0a4;
constexpr std::uint64_t kRegConfigGeneration = 0x0fc;
constexpr std::uint64_t kRegConfig = 0x100;

constexpr std::uint32_t kMagicValue = ('v' | ('i' << 8) | ('r' << 16) | ('t' << 24));
constexpr std::uint32_t kVersionModern = 2u;
constexpr std::uint32_t kDeviceIdNet = 1u;
constexpr std::uint32_t kVendorId = 0x41564d31u; // "AVM1"

constexpr std::uint64_t kFeatureMac = 1ull << 5;
constexpr std::uint64_t kFeatureMrgRxbuf = 1ull << 15;
constexpr std::uint64_t kFeatureStatus = 1ull << 16;
constexpr std::uint64_t kFeatureVersion1 = 1ull << 32;

constexpr std::uint16_t kVringDescFNext = 1u;
constexpr std::uint16_t kVringDescFWrite = 2u;
constexpr std::uint16_t kVringDescFIndirect = 4u;

constexpr std::uint32_t kInterruptVring = 1u << 0;

constexpr std::size_t kConfigMacOffset = 0u;
constexpr std::size_t kConfigStatusOffset = 6u;
constexpr std::uint16_t kNetStatusLinkUp = 1u;

template <typename T>
bool access_matches(std::uint64_t offset, std::size_t size, std::uint64_t reg_offset) {
  return offset == reg_offset && size == sizeof(T);
}

template <typename T>
T load_le(const std::uint8_t* src) {
  T value{};
  std::memcpy(&value, src, sizeof(T));
  return value;
}

template <typename T>
void store_le(std::uint8_t* dst, T value) {
  std::memcpy(dst, &value, sizeof(T));
}

bool looks_like_ethernet_frame(const std::uint8_t* data, std::size_t len) {
  if (len < 14u) {
    return false;
  }
  const std::uint16_t type_or_len = static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[12]) << 8) | data[13]);
  return type_or_len >= 0x0600u;
}

} // namespace

VirtioNetMmio::VirtioNetMmio(Bus& bus) : bus_(bus) {}

std::uint64_t VirtioNetMmio::read(std::uint64_t offset, std::size_t size) {
  if (offset >= kRegConfig && offset < (kRegConfig + kConfigSize)) {
    return read_config(offset - kRegConfig, size);
  }

  if (access_matches<std::uint32_t>(offset, size, kRegMagicValue)) {
    return kMagicValue;
  }
  if (access_matches<std::uint32_t>(offset, size, kRegVersion)) {
    return kVersionModern;
  }
  if (access_matches<std::uint32_t>(offset, size, kRegDeviceId)) {
    return device_present() ? kDeviceIdNet : 0u;
  }
  if (access_matches<std::uint32_t>(offset, size, kRegVendorId)) {
    return kVendorId;
  }
  if (access_matches<std::uint32_t>(offset, size, kRegDeviceFeatures)) {
    return device_features_word(device_features_sel_);
  }
  if (access_matches<std::uint32_t>(offset, size, kRegQueueNumMax)) {
    return (device_present() && queue_sel_ < kQueueCount) ? kQueueSize : 0u;
  }
  if (access_matches<std::uint32_t>(offset, size, kRegInterruptStatus)) {
    return interrupt_status_;
  }
  if (access_matches<std::uint32_t>(offset, size, kRegStatus)) {
    return status_;
  }
  if (access_matches<std::uint32_t>(offset, size, kRegConfigGeneration)) {
    return config_generation_;
  }

  return read_queue_register(offset, size);
}

void VirtioNetMmio::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  if (offset >= kRegConfig && offset < (kRegConfig + kConfigSize)) {
    return;
  }

  if (access_matches<std::uint32_t>(offset, size, kRegDeviceFeaturesSel)) {
    device_features_sel_ = static_cast<std::uint32_t>(value);
    return;
  }
  if (access_matches<std::uint32_t>(offset, size, kRegDriverFeaturesSel)) {
    driver_features_sel_ = static_cast<std::uint32_t>(value);
    return;
  }
  if (access_matches<std::uint32_t>(offset, size, kRegDriverFeatures)) {
    const std::uint32_t selector = std::min(driver_features_sel_, 1u);
    const std::uint64_t mask = 0xffffffffull << (selector * 32u);
    driver_features_ = (driver_features_ & ~mask) |
                       ((static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)) << (selector * 32u)) & mask);
    return;
  }
  if (access_matches<std::uint32_t>(offset, size, kRegGuestPageSize)) {
    guest_page_size_ = static_cast<std::uint32_t>(value);
    return;
  }
  if (access_matches<std::uint32_t>(offset, size, kRegInterruptAck)) {
    clear_interrupt(static_cast<std::uint32_t>(value));
    return;
  }
  if (access_matches<std::uint32_t>(offset, size, kRegStatus)) {
    const std::uint32_t next_status = static_cast<std::uint32_t>(value);
    if (next_status == 0u) {
      reset_device_state();
    } else {
      status_ = next_status;
    }
    return;
  }

  write_queue_register(offset, value, size);
}

void VirtioNetMmio::attach_host_backend(std::array<std::uint8_t, 6> mac) {
  mac_ = mac;
  present_ = true;
  loopback_enabled_ = false;
  pending_rx_frames_.clear();
  guest_header_size_ = kVirtioNetHdrSize;
  ++config_generation_;
  reset_device_state();
}

void VirtioNetMmio::attach_loopback(std::array<std::uint8_t, 6> mac) {
  mac_ = mac;
  present_ = true;
  loopback_enabled_ = true;
  pending_rx_frames_.clear();
  guest_header_size_ = kVirtioNetHdrSize;
  ++config_generation_;
  reset_device_state();
}

void VirtioNetMmio::detach() {
  present_ = false;
  loopback_enabled_ = false;
  pending_rx_frames_.clear();
  guest_header_size_ = kVirtioNetHdrSize;
  ++config_generation_;
  reset_device_state();
}

bool VirtioNetMmio::enqueue_rx_frame(std::vector<std::uint8_t> frame) {
  if (!device_present()) {
    return false;
  }
  pending_rx_frames_.push_back(std::move(frame));
  if (process_rx()) {
    raise_interrupt(kInterruptVring);
  }
  return true;
}

bool VirtioNetMmio::save_state(std::ostream& out) const {
  if (!snapshot_io::write_bool(out, present_) ||
      !snapshot_io::write_bool(out, loopback_enabled_) ||
      !snapshot_io::write_array(out, mac_) ||
      !snapshot_io::write(out, device_features_sel_) ||
      !snapshot_io::write(out, driver_features_sel_) ||
      !snapshot_io::write(out, driver_features_) ||
      !snapshot_io::write(out, guest_page_size_) ||
      !snapshot_io::write(out, queue_sel_) ||
      !snapshot_io::write(out, queues_) ||
      !snapshot_io::write(out, interrupt_status_) ||
      !snapshot_io::write(out, status_) ||
      !snapshot_io::write(out, config_generation_)) {
    return false;
  }

  const std::uint64_t pending_count = static_cast<std::uint64_t>(pending_rx_frames_.size());
  if (!snapshot_io::write(out, pending_count)) {
    return false;
  }
  for (const auto& frame : pending_rx_frames_) {
    if (!snapshot_io::write_vector(out, frame)) {
      return false;
    }
  }
  return true;
}

bool VirtioNetMmio::load_state(std::istream& in) {
  std::array<QueueState, kQueueCount> queues{};
  std::deque<std::vector<std::uint8_t>> pending_frames;
  if (!snapshot_io::read_bool(in, present_) ||
      !snapshot_io::read_bool(in, loopback_enabled_) ||
      !snapshot_io::read_array(in, mac_) ||
      !snapshot_io::read(in, device_features_sel_) ||
      !snapshot_io::read(in, driver_features_sel_) ||
      !snapshot_io::read(in, driver_features_) ||
      !snapshot_io::read(in, guest_page_size_) ||
      !snapshot_io::read(in, queue_sel_) ||
      !snapshot_io::read(in, queues) ||
      !snapshot_io::read(in, interrupt_status_) ||
      !snapshot_io::read(in, status_) ||
      !snapshot_io::read(in, config_generation_)) {
    return false;
  }

  std::uint64_t pending_count = 0;
  if (!snapshot_io::read(in, pending_count) || pending_count > 1024u) {
    return false;
  }
  for (std::uint64_t i = 0; i < pending_count; ++i) {
    std::vector<std::uint8_t> frame;
    if (!snapshot_io::read_vector(in, frame)) {
      return false;
    }
    pending_frames.push_back(std::move(frame));
  }

  if (device_features_sel_ > 1u || driver_features_sel_ > 1u || queue_sel_ >= kQueueCount) {
    return false;
  }
  for (const auto& queue : queues) {
    if (queue.num > kQueueSize) {
      return false;
    }
  }

  queues_ = queues;
  guest_header_size_ = net_header_size();
  pending_rx_frames_ = std::move(pending_frames);
  return true;
}

void VirtioNetMmio::reset_device_state() {
  device_features_sel_ = 0;
  driver_features_sel_ = 0;
  driver_features_ = 0;
  guest_header_size_ = kVirtioNetHdrSize;
  guest_page_size_ = 4096u;
  queue_sel_ = 0;
  reset_queue_state();
  status_ = 0;
  clear_interrupt(~0u);
}

void VirtioNetMmio::reset_queue_state() {
  queues_ = {};
}

void VirtioNetMmio::notify_state_change() const {
  if (state_change_observer_) {
    state_change_observer_();
  }
}

void VirtioNetMmio::raise_interrupt(std::uint32_t bits) {
  const std::uint32_t next = interrupt_status_ | bits;
  if (next != interrupt_status_) {
    interrupt_status_ = next;
    notify_state_change();
  }
}

void VirtioNetMmio::clear_interrupt(std::uint32_t bits) {
  const std::uint32_t next = interrupt_status_ & ~bits;
  if (next != interrupt_status_) {
    interrupt_status_ = next;
    notify_state_change();
  }
}

std::uint64_t VirtioNetMmio::read_config(std::uint64_t offset, std::size_t size) const {
  if (!device_present() || size == 0u || size > 8u || (offset + size) > kConfigSize) {
    return 0;
  }

  std::array<std::uint8_t, kConfigSize> config{};
  std::memcpy(config.data() + kConfigMacOffset, mac_.data(), mac_.size());
  store_le<std::uint16_t>(config.data() + kConfigStatusOffset, kNetStatusLinkUp);

  std::uint64_t value = 0;
  std::memcpy(&value, config.data() + offset, size);
  return value;
}

std::uint64_t VirtioNetMmio::read_queue_register(std::uint64_t offset, std::size_t size) const {
  if (!access_matches<std::uint32_t>(offset, size, kRegQueueSel) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueNum) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueReady) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueDescLow) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueDescHigh) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueAvailLow) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueAvailHigh) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueUsedLow) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueUsedHigh)) {
    return 0;
  }

  if (offset == kRegQueueSel) {
    return queue_sel_;
  }
  if (queue_sel_ >= kQueueCount) {
    return 0;
  }

  const QueueState& queue = queues_[queue_sel_];
  if (offset == kRegQueueNum) {
    return queue.num;
  }
  if (offset == kRegQueueReady) {
    return queue.ready ? 1u : 0u;
  }
  if (offset == kRegQueueDescLow) {
    return static_cast<std::uint32_t>(queue.desc_addr);
  }
  if (offset == kRegQueueDescHigh) {
    return static_cast<std::uint32_t>(queue.desc_addr >> 32);
  }
  if (offset == kRegQueueAvailLow) {
    return static_cast<std::uint32_t>(queue.avail_addr);
  }
  if (offset == kRegQueueAvailHigh) {
    return static_cast<std::uint32_t>(queue.avail_addr >> 32);
  }
  if (offset == kRegQueueUsedLow) {
    return static_cast<std::uint32_t>(queue.used_addr);
  }
  if (offset == kRegQueueUsedHigh) {
    return static_cast<std::uint32_t>(queue.used_addr >> 32);
  }
  return 0;
}

void VirtioNetMmio::write_queue_register(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  if (!access_matches<std::uint32_t>(offset, size, kRegQueueSel) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueNum) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueReady) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueNotify) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueDescLow) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueDescHigh) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueAvailLow) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueAvailHigh) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueUsedLow) &&
      !access_matches<std::uint32_t>(offset, size, kRegQueueUsedHigh)) {
    return;
  }

  if (offset == kRegQueueSel) {
    queue_sel_ = static_cast<std::uint32_t>(value);
    return;
  }
  if (offset == kRegQueueNotify) {
    process_queue(static_cast<std::uint32_t>(value));
    return;
  }
  if (queue_sel_ >= kQueueCount) {
    return;
  }

  QueueState& queue = queues_[queue_sel_];
  switch (offset) {
    case kRegQueueNum:
      queue.num = std::min(static_cast<std::uint32_t>(value), kQueueSize);
      break;
    case kRegQueueReady:
      queue.ready = (value != 0u);
      if (!queue.ready) {
        queue.last_avail_idx = 0;
        queue.used_idx = 0;
      }
      break;
    case kRegQueueDescLow:
      queue.desc_addr = (queue.desc_addr & 0xffffffff00000000ull) | static_cast<std::uint32_t>(value);
      break;
    case kRegQueueDescHigh:
      queue.desc_addr = (queue.desc_addr & 0x00000000ffffffffull) |
                        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)) << 32);
      break;
    case kRegQueueAvailLow:
      queue.avail_addr = (queue.avail_addr & 0xffffffff00000000ull) | static_cast<std::uint32_t>(value);
      break;
    case kRegQueueAvailHigh:
      queue.avail_addr = (queue.avail_addr & 0x00000000ffffffffull) |
                         (static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)) << 32);
      break;
    case kRegQueueUsedLow:
      queue.used_addr = (queue.used_addr & 0xffffffff00000000ull) | static_cast<std::uint32_t>(value);
      break;
    case kRegQueueUsedHigh:
      queue.used_addr = (queue.used_addr & 0x00000000ffffffffull) |
                        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)) << 32);
      break;
    default:
      break;
  }
}

std::uint64_t VirtioNetMmio::device_features() const {
  if (!device_present()) {
    return 0;
  }
  return kFeatureMac | kFeatureMrgRxbuf | kFeatureStatus | kFeatureVersion1;
}

std::uint32_t VirtioNetMmio::device_features_word(std::uint32_t selector) const {
  const std::uint64_t features = device_features();
  if (selector == 0u) {
    return static_cast<std::uint32_t>(features);
  }
  if (selector == 1u) {
    return static_cast<std::uint32_t>(features >> 32);
  }
  return 0u;
}

std::size_t VirtioNetMmio::net_header_size() const {
  return (driver_features_ & kFeatureMrgRxbuf) != 0u ? kVirtioNetMrgRxbufHdrSize : kVirtioNetHdrSize;
}

std::size_t VirtioNetMmio::active_header_size() const {
  return std::max(net_header_size(), guest_header_size_);
}

std::size_t VirtioNetMmio::detect_tx_header_size(const std::vector<TransferSegment>& segments,
                                                 const std::vector<std::uint8_t>& chain_bytes) const {
  if (guest_header_size_ == kVirtioNetMrgRxbufHdrSize && chain_bytes.size() >= kVirtioNetMrgRxbufHdrSize) {
    return kVirtioNetMrgRxbufHdrSize;
  }
  if (net_header_size() == kVirtioNetMrgRxbufHdrSize && chain_bytes.size() >= kVirtioNetMrgRxbufHdrSize) {
    return kVirtioNetMrgRxbufHdrSize;
  }
  if (!segments.empty() &&
      segments.front().len >= kVirtioNetMrgRxbufHdrSize &&
      chain_bytes.size() >= kVirtioNetMrgRxbufHdrSize &&
      std::all_of(chain_bytes.begin(),
                  chain_bytes.begin() + static_cast<std::ptrdiff_t>(kVirtioNetMrgRxbufHdrSize),
                  [](std::uint8_t byte) { return byte == 0u; }) &&
      looks_like_ethernet_frame(chain_bytes.data() + kVirtioNetMrgRxbufHdrSize,
                                chain_bytes.size() - kVirtioNetMrgRxbufHdrSize)) {
    return kVirtioNetMrgRxbufHdrSize;
  }
  return kVirtioNetHdrSize;
}

void VirtioNetMmio::process_queue(std::uint32_t queue_index) {
  if (!device_present() || queue_index >= kQueueCount) {
    return;
  }

  bool completed = false;
  if (queue_index == kQueueTx) {
    completed = process_tx();
    if (!pending_rx_frames_.empty()) {
      completed = process_rx() || completed;
    }
  } else if (queue_index == kQueueRx) {
    completed = process_rx();
  }

  if (completed) {
    raise_interrupt(kInterruptVring);
  }
}

bool VirtioNetMmio::process_tx() {
  QueueState& queue = queues_[kQueueTx];
  if (!queue.ready || queue.num == 0u || queue.desc_addr == 0u || queue.avail_addr == 0u || queue.used_addr == 0u) {
    return false;
  }

  std::uint16_t avail_idx = 0;
  if (!read_guest_u16(queue.avail_addr + 2u, avail_idx)) {
    return false;
  }

  bool any_completed = false;
  while (queue.last_avail_idx != avail_idx) {
    const std::uint64_t ring_addr =
        queue.avail_addr + 4u + static_cast<std::uint64_t>((queue.last_avail_idx % queue.num) * sizeof(std::uint16_t));
    std::uint16_t head_index = 0;
    if (!read_guest_u16(ring_addr, head_index)) {
      break;
    }

    std::vector<TransferSegment> segments;
    if (!collect_chain(queue, head_index, segments) || segments.empty()) {
      break;
    }

    std::size_t total_len = 0;
    bool invalid = false;
    for (const auto& segment : segments) {
      if (segment.writable) {
        invalid = true;
        break;
      }
      total_len += segment.len;
    }
    if (invalid || total_len < kVirtioNetHdrSize) {
      break;
    }

    std::vector<std::uint8_t> chain_bytes(total_len);
    std::size_t copied = 0;
    for (const auto& segment : segments) {
      if (!read_guest(segment.addr, chain_bytes.data() + copied, segment.len)) {
        invalid = true;
        break;
      }
      copied += segment.len;
    }
    if (invalid) {
      break;
    }

    const std::size_t net_hdr_size = detect_tx_header_size(segments, chain_bytes);
    std::vector<std::uint8_t> frame(chain_bytes.begin() + static_cast<std::ptrdiff_t>(net_hdr_size),
                                    chain_bytes.end());
    guest_header_size_ = net_hdr_size;
    if (net_hdr_size == kVirtioNetMrgRxbufHdrSize) {
      // Some mainline guests emit the 12-byte merged-rx header shape even if they did not
      // explicitly leave the negotiation bit set in the legacy register model. Persist the
      // observed width through snapshot/restore by upgrading the internal effective feature set.
      driver_features_ |= kFeatureMrgRxbuf;
    }
    if (loopback_enabled_) {
      pending_rx_frames_.push_back(frame);
    } else if (tx_frame_handler_) {
      tx_frame_handler_(std::move(frame));
    }

    if (!write_used(queue, head_index, 0u, queue.used_idx)) {
      break;
    }
    ++queue.last_avail_idx;
    ++queue.used_idx;
    if (!write_guest_u16(queue.used_addr + 2u, queue.used_idx)) {
      break;
    }
    any_completed = true;
  }

  return any_completed;
}

bool VirtioNetMmio::process_rx() {
  QueueState& queue = queues_[kQueueRx];
  const std::size_t net_hdr_size = active_header_size();
  if (pending_rx_frames_.empty() || !queue.ready || queue.num == 0u || queue.desc_addr == 0u ||
      queue.avail_addr == 0u || queue.used_addr == 0u) {
    return false;
  }

  std::uint16_t avail_idx = 0;
  if (!read_guest_u16(queue.avail_addr + 2u, avail_idx)) {
    return false;
  }

  bool any_completed = false;
  while (queue.last_avail_idx != avail_idx && !pending_rx_frames_.empty()) {
    const std::uint64_t ring_addr =
        queue.avail_addr + 4u + static_cast<std::uint64_t>((queue.last_avail_idx % queue.num) * sizeof(std::uint16_t));
    std::uint16_t head_index = 0;
    if (!read_guest_u16(ring_addr, head_index)) {
      break;
    }

    std::vector<TransferSegment> segments;
    if (!collect_chain(queue, head_index, segments) || segments.empty()) {
      break;
    }

    std::size_t capacity = 0;
    bool invalid = false;
    for (const auto& segment : segments) {
      if (!segment.writable) {
        invalid = true;
        break;
      }
      capacity += segment.len;
    }
    if (invalid) {
      break;
    }

    const std::vector<std::uint8_t>& frame = pending_rx_frames_.front();
    const std::size_t total_len = net_hdr_size + frame.size();
    if (capacity < total_len) {
      break;
    }

    std::vector<std::uint8_t> payload(total_len, 0u);
    if (net_hdr_size == kVirtioNetMrgRxbufHdrSize) {
      payload[kVirtioNetHdrSize + 0u] = 1u;
      payload[kVirtioNetHdrSize + 1u] = 0u;
    }
    if (!frame.empty()) {
      std::memcpy(payload.data() + net_hdr_size, frame.data(), frame.size());
    }

    std::size_t remaining = total_len;
    std::size_t offset = 0;
    for (const auto& segment : segments) {
      if (remaining == 0u) {
        break;
      }
      const std::size_t chunk = std::min<std::size_t>(segment.len, remaining);
      if (!write_guest(segment.addr, payload.data() + offset, chunk)) {
        invalid = true;
        break;
      }
      remaining -= chunk;
      offset += chunk;
    }
    if (invalid || remaining != 0u) {
      break;
    }

    if (!write_used(queue, head_index, static_cast<std::uint32_t>(total_len), queue.used_idx)) {
      break;
    }
    pending_rx_frames_.pop_front();
    ++queue.last_avail_idx;
    ++queue.used_idx;
    if (!write_guest_u16(queue.used_addr + 2u, queue.used_idx)) {
      break;
    }
    any_completed = true;
  }

  return any_completed;
}

bool VirtioNetMmio::read_descriptor(const QueueState& queue,
                                    std::uint16_t index,
                                    std::uint64_t& addr,
                                    std::uint32_t& len,
                                    std::uint16_t& flags,
                                    std::uint16_t& next) const {
  if (queue.num == 0u || index >= queue.num) {
    return false;
  }
  std::array<std::uint8_t, 16> desc{};
  const std::uint64_t desc_addr = queue.desc_addr + static_cast<std::uint64_t>(index) * desc.size();
  if (!read_guest(desc_addr, desc.data(), desc.size())) {
    return false;
  }
  addr = load_le<std::uint64_t>(desc.data() + 0u);
  len = load_le<std::uint32_t>(desc.data() + 8u);
  flags = load_le<std::uint16_t>(desc.data() + 12u);
  next = load_le<std::uint16_t>(desc.data() + 14u);
  return true;
}

bool VirtioNetMmio::collect_chain(const QueueState& queue,
                                  std::uint16_t head_index,
                                  std::vector<TransferSegment>& segments) const {
  segments.clear();
  std::uint16_t index = head_index;
  std::uint32_t hops = 0;
  while (true) {
    if (++hops > queue.num) {
      return false;
    }

    std::uint64_t addr = 0;
    std::uint32_t len = 0;
    std::uint16_t flags = 0;
    std::uint16_t next = 0;
    if (!read_descriptor(queue, index, addr, len, flags, next) || (flags & kVringDescFIndirect) != 0u) {
      return false;
    }

    segments.push_back(TransferSegment{
        .addr = addr,
        .len = len,
        .writable = (flags & kVringDescFWrite) != 0u,
    });
    if ((flags & kVringDescFNext) == 0u) {
      return true;
    }
    index = next;
  }
}

bool VirtioNetMmio::write_used(const QueueState& queue,
                               std::uint16_t head_index,
                               std::uint32_t used_len,
                               std::uint16_t used_idx) {
  const std::uint64_t used_elem_addr =
      queue.used_addr + 4u + static_cast<std::uint64_t>((used_idx % queue.num) * 8u);
  return write_guest_u32(used_elem_addr + 0u, head_index) &&
         write_guest_u32(used_elem_addr + 4u, used_len);
}

bool VirtioNetMmio::read_guest(std::uint64_t addr, void* dst, std::size_t size) const {
  if (size == 0u) {
    return true;
  }
  const std::uint8_t* src = bus_.ram_ptr(addr, size);
  if (src == nullptr) {
    return false;
  }
  std::memcpy(dst, src, size);
  return true;
}

bool VirtioNetMmio::write_guest(std::uint64_t addr, const void* src, std::size_t size) {
  return bus_.write_ram_buffer(addr, src, size);
}

bool VirtioNetMmio::read_guest_u16(std::uint64_t addr, std::uint16_t& value) const {
  std::array<std::uint8_t, sizeof(value)> bytes{};
  if (!read_guest(addr, bytes.data(), bytes.size())) {
    return false;
  }
  value = load_le<std::uint16_t>(bytes.data());
  return true;
}

bool VirtioNetMmio::write_guest_u16(std::uint64_t addr, std::uint16_t value) {
  std::array<std::uint8_t, sizeof(value)> bytes{};
  store_le<std::uint16_t>(bytes.data(), value);
  return write_guest(addr, bytes.data(), bytes.size());
}

bool VirtioNetMmio::write_guest_u32(std::uint64_t addr, std::uint32_t value) {
  std::array<std::uint8_t, sizeof(value)> bytes{};
  store_le<std::uint32_t>(bytes.data(), value);
  return write_guest(addr, bytes.data(), bytes.size());
}

} // namespace aarchvm
