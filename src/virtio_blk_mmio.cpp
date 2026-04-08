#include "aarchvm/virtio_blk_mmio.hpp"

#include "aarchvm/bus.hpp"
#include "aarchvm/snapshot_io.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>
#include <vector>

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
constexpr std::uint32_t kDeviceIdBlock = 2u;
constexpr std::uint32_t kVendorId = 0x41564d31u; // "AVM1"

constexpr std::uint64_t kFeatureVersion1 = 1ull << 32;
constexpr std::uint64_t kFeatureBlkSize = 1ull << 6;

constexpr std::uint16_t kVringDescFNext = 1u;
constexpr std::uint16_t kVringDescFWrite = 2u;
constexpr std::uint16_t kVringDescFIndirect = 4u;

constexpr std::uint32_t kInterruptVring = 1u << 0;

constexpr std::uint32_t kReqTypeIn = 0u;
constexpr std::uint32_t kReqTypeOut = 1u;
constexpr std::uint32_t kReqTypeFlush = 4u;
constexpr std::uint32_t kReqTypeGetId = 8u;

constexpr std::uint8_t kReqStatusOk = 0u;
constexpr std::uint8_t kReqStatusIoErr = 1u;
constexpr std::uint8_t kReqStatusUnsupported = 2u;

constexpr std::size_t kConfigCapacityOffset = 0u;
constexpr std::size_t kConfigBlkSizeOffset = 20u;

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

} // namespace

VirtioBlkMmio::VirtioBlkMmio(Bus& bus) : bus_(bus) {}

std::uint64_t VirtioBlkMmio::read(std::uint64_t offset, std::size_t size) {
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
    return device_present() ? kDeviceIdBlock : 0u;
  }
  if (access_matches<std::uint32_t>(offset, size, kRegVendorId)) {
    return kVendorId;
  }
  if (access_matches<std::uint32_t>(offset, size, kRegDeviceFeatures)) {
    return device_features_word(device_features_sel_);
  }
  if (access_matches<std::uint32_t>(offset, size, kRegQueueNumMax)) {
    return (device_present() && queue_sel_ == 0u) ? kQueueSize : 0u;
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

void VirtioBlkMmio::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
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

void VirtioBlkMmio::set_image(std::vector<std::uint8_t> bytes) {
  const std::size_t padded_size =
      ((bytes.size() + static_cast<std::size_t>(kSectorSize) - 1u) / static_cast<std::size_t>(kSectorSize)) *
      static_cast<std::size_t>(kSectorSize);
  bytes.resize(padded_size, 0);
  image_ = std::move(bytes);
  ++config_generation_;
  reset_device_state();
}

std::uint64_t VirtioBlkMmio::num_blocks() const {
  return static_cast<std::uint64_t>(image_.size() / kSectorSize);
}

bool VirtioBlkMmio::save_state(std::ostream& out) const {
  return snapshot_io::write_vector(out, image_) &&
         snapshot_io::write(out, device_features_sel_) &&
         snapshot_io::write(out, driver_features_sel_) &&
         snapshot_io::write(out, driver_features_) &&
         snapshot_io::write(out, guest_page_size_) &&
         snapshot_io::write(out, queue_sel_) &&
         snapshot_io::write(out, queue_.num) &&
         snapshot_io::write_bool(out, queue_.ready) &&
         snapshot_io::write(out, queue_.desc_addr) &&
         snapshot_io::write(out, queue_.avail_addr) &&
         snapshot_io::write(out, queue_.used_addr) &&
         snapshot_io::write(out, queue_.last_avail_idx) &&
         snapshot_io::write(out, queue_.used_idx) &&
         snapshot_io::write(out, interrupt_status_) &&
         snapshot_io::write(out, status_) &&
         snapshot_io::write(out, config_generation_);
}

bool VirtioBlkMmio::load_state(std::istream& in) {
  QueueState queue{};
  if (!snapshot_io::read_vector(in, image_) ||
      !snapshot_io::read(in, device_features_sel_) ||
      !snapshot_io::read(in, driver_features_sel_) ||
      !snapshot_io::read(in, driver_features_) ||
      !snapshot_io::read(in, guest_page_size_) ||
      !snapshot_io::read(in, queue_sel_) ||
      !snapshot_io::read(in, queue.num) ||
      !snapshot_io::read_bool(in, queue.ready) ||
      !snapshot_io::read(in, queue.desc_addr) ||
      !snapshot_io::read(in, queue.avail_addr) ||
      !snapshot_io::read(in, queue.used_addr) ||
      !snapshot_io::read(in, queue.last_avail_idx) ||
      !snapshot_io::read(in, queue.used_idx) ||
      !snapshot_io::read(in, interrupt_status_) ||
      !snapshot_io::read(in, status_) ||
      !snapshot_io::read(in, config_generation_)) {
    return false;
  }
  if ((image_.size() % kSectorSize) != 0u ||
      queue_sel_ > 0u ||
      queue.num > kQueueSize ||
      device_features_sel_ > 1u ||
      driver_features_sel_ > 1u) {
    return false;
  }
  queue_ = queue;
  return true;
}

bool VirtioBlkMmio::load_legacy_block_mmio_state(std::istream& in) {
  std::vector<std::uint8_t> image;
  std::uint64_t buffer_addr = 0;
  std::uint64_t lba = 0;
  std::uint32_t count = 0;
  std::uint32_t status = 0;
  if (!snapshot_io::read_vector(in, image) ||
      !snapshot_io::read(in, buffer_addr) ||
      !snapshot_io::read(in, lba) ||
      !snapshot_io::read(in, count) ||
      !snapshot_io::read(in, status)) {
    return false;
  }
  (void)buffer_addr;
  (void)lba;
  (void)count;
  if ((image.size() % kSectorSize) != 0u || status > 4u) {
    return false;
  }
  set_image(std::move(image));
  return true;
}

void VirtioBlkMmio::reset_device_state() {
  device_features_sel_ = 0;
  driver_features_sel_ = 0;
  driver_features_ = 0;
  guest_page_size_ = 4096u;
  queue_sel_ = 0;
  reset_queue_state();
  status_ = 0;
  clear_interrupt(~0u);
}

void VirtioBlkMmio::reset_queue_state() {
  queue_ = {};
}

void VirtioBlkMmio::notify_state_change() const {
  if (state_change_observer_) {
    state_change_observer_();
  }
}

void VirtioBlkMmio::raise_interrupt(std::uint32_t bits) {
  const std::uint32_t next = interrupt_status_ | bits;
  if (next != interrupt_status_) {
    interrupt_status_ = next;
    notify_state_change();
  }
}

void VirtioBlkMmio::clear_interrupt(std::uint32_t bits) {
  const std::uint32_t next = interrupt_status_ & ~bits;
  if (next != interrupt_status_) {
    interrupt_status_ = next;
    notify_state_change();
  }
}

std::uint64_t VirtioBlkMmio::read_config(std::uint64_t offset, std::size_t size) const {
  if (!device_present() || size == 0u || size > 8u || (offset + size) > kConfigSize) {
    return 0;
  }

  std::array<std::uint8_t, kConfigSize> config{};
  store_le<std::uint64_t>(config.data() + kConfigCapacityOffset, num_blocks());
  store_le<std::uint32_t>(config.data() + kConfigBlkSizeOffset, kSectorSize);

  std::uint64_t value = 0;
  std::memcpy(&value, config.data() + offset, size);
  return value;
}

std::uint64_t VirtioBlkMmio::read_queue_register(std::uint64_t offset, std::size_t size) const {
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
  if (queue_sel_ != 0u) {
    return 0;
  }
  if (offset == kRegQueueNum) {
    return queue_.num;
  }
  if (offset == kRegQueueReady) {
    return queue_.ready ? 1u : 0u;
  }
  if (offset == kRegQueueDescLow) {
    return static_cast<std::uint32_t>(queue_.desc_addr);
  }
  if (offset == kRegQueueDescHigh) {
    return static_cast<std::uint32_t>(queue_.desc_addr >> 32);
  }
  if (offset == kRegQueueAvailLow) {
    return static_cast<std::uint32_t>(queue_.avail_addr);
  }
  if (offset == kRegQueueAvailHigh) {
    return static_cast<std::uint32_t>(queue_.avail_addr >> 32);
  }
  if (offset == kRegQueueUsedLow) {
    return static_cast<std::uint32_t>(queue_.used_addr);
  }
  if (offset == kRegQueueUsedHigh) {
    return static_cast<std::uint32_t>(queue_.used_addr >> 32);
  }
  return 0;
}

void VirtioBlkMmio::write_queue_register(std::uint64_t offset, std::uint64_t value, std::size_t size) {
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
  if (queue_sel_ != 0u) {
    return;
  }

  switch (offset) {
    case kRegQueueNum:
      queue_.num = std::min(static_cast<std::uint32_t>(value), kQueueSize);
      break;
    case kRegQueueReady:
      queue_.ready = (value != 0u);
      if (!queue_.ready) {
        queue_.last_avail_idx = 0;
        queue_.used_idx = 0;
      }
      break;
    case kRegQueueDescLow:
      queue_.desc_addr = (queue_.desc_addr & 0xffffffff00000000ull) | (static_cast<std::uint32_t>(value));
      break;
    case kRegQueueDescHigh:
      queue_.desc_addr =
          (queue_.desc_addr & 0x00000000ffffffffull) | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)) << 32);
      break;
    case kRegQueueAvailLow:
      queue_.avail_addr = (queue_.avail_addr & 0xffffffff00000000ull) | (static_cast<std::uint32_t>(value));
      break;
    case kRegQueueAvailHigh:
      queue_.avail_addr =
          (queue_.avail_addr & 0x00000000ffffffffull) | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)) << 32);
      break;
    case kRegQueueUsedLow:
      queue_.used_addr = (queue_.used_addr & 0xffffffff00000000ull) | (static_cast<std::uint32_t>(value));
      break;
    case kRegQueueUsedHigh:
      queue_.used_addr =
          (queue_.used_addr & 0x00000000ffffffffull) | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)) << 32);
      break;
    default:
      break;
  }
}

std::uint64_t VirtioBlkMmio::device_features() const {
  if (!device_present()) {
    return 0;
  }
  return kFeatureVersion1 | kFeatureBlkSize;
}

std::uint32_t VirtioBlkMmio::device_features_word(std::uint32_t selector) const {
  const std::uint64_t features = device_features();
  if (selector == 0u) {
    return static_cast<std::uint32_t>(features);
  }
  if (selector == 1u) {
    return static_cast<std::uint32_t>(features >> 32);
  }
  return 0u;
}

void VirtioBlkMmio::process_queue(std::uint32_t queue_index) {
  if (queue_index != 0u || !device_present() || !queue_.ready || queue_.num == 0u ||
      queue_.desc_addr == 0u || queue_.avail_addr == 0u || queue_.used_addr == 0u) {
    return;
  }

  std::uint16_t avail_idx = 0;
  if (!read_guest_u16(queue_.avail_addr + 2u, avail_idx)) {
    return;
  }

  bool any_completed = false;
  while (queue_.last_avail_idx != avail_idx) {
    const std::uint64_t ring_addr =
        queue_.avail_addr + 4u + static_cast<std::uint64_t>((queue_.last_avail_idx % queue_.num) * sizeof(std::uint16_t));
    std::uint16_t head_index = 0;
    if (!read_guest_u16(ring_addr, head_index)) {
      break;
    }

    std::uint32_t used_len = 0;
    std::uint8_t status_byte = kReqStatusIoErr;
    if (!process_request(head_index, used_len, status_byte)) {
      used_len = 1u;
    }

    const std::uint64_t used_elem_addr =
        queue_.used_addr + 4u + static_cast<std::uint64_t>((queue_.used_idx % queue_.num) * 8u);
    if (!write_guest_u32(used_elem_addr + 0u, head_index) ||
        !write_guest_u32(used_elem_addr + 4u, used_len)) {
      break;
    }

    ++queue_.last_avail_idx;
    ++queue_.used_idx;
    if (!write_guest_u16(queue_.used_addr + 2u, queue_.used_idx)) {
      break;
    }
    any_completed = true;
  }

  if (any_completed) {
    raise_interrupt(kInterruptVring);
  }
}

bool VirtioBlkMmio::process_request(std::uint16_t head_index, std::uint32_t& used_len, std::uint8_t& status_byte) {
  used_len = 0;
  status_byte = kReqStatusIoErr;

  std::uint64_t hdr_addr = 0;
  std::uint32_t hdr_len = 0;
  std::uint16_t hdr_flags = 0;
  std::uint16_t next_index = 0;
  if (!read_descriptor(head_index, hdr_addr, hdr_len, hdr_flags, next_index) ||
      hdr_len < 16u || (hdr_flags & kVringDescFWrite) != 0u || (hdr_flags & kVringDescFIndirect) != 0u ||
      (hdr_flags & kVringDescFNext) == 0u) {
    return false;
  }

  std::array<std::uint8_t, 16> hdr{};
  if (!read_guest(hdr_addr, hdr.data(), hdr.size())) {
    return false;
  }
  const std::uint32_t type = load_le<std::uint32_t>(hdr.data() + 0u);
  const std::uint64_t sector = load_le<std::uint64_t>(hdr.data() + 8u);

  std::vector<TransferSegment> segments;
  std::uint64_t status_addr = 0;
  std::uint32_t status_len = 0;
  std::uint16_t index = next_index;
  std::uint32_t hops = 0;
  while (true) {
    if (++hops > queue_.num) {
      return false;
    }

    std::uint64_t addr = 0;
    std::uint32_t len = 0;
    std::uint16_t flags = 0;
    std::uint16_t next = 0;
    if (!read_descriptor(index, addr, len, flags, next) || (flags & kVringDescFIndirect) != 0u) {
      return false;
    }

    const bool has_next = (flags & kVringDescFNext) != 0u;
    if (!has_next) {
      if ((flags & kVringDescFWrite) == 0u || len == 0u) {
        return false;
      }
      status_addr = addr;
      status_len = len;
      break;
    }

    segments.push_back(TransferSegment{
        .addr = addr,
        .len = len,
        .writable = (flags & kVringDescFWrite) != 0u,
    });
    index = next;
  }

  auto write_status = [&](std::uint8_t byte) -> bool {
    status_byte = byte;
    return status_len > 0u && write_guest(status_addr, &status_byte, 1u);
  };

  const auto total_data_bytes = [&segments]() {
    std::uint64_t total = 0;
    for (const auto& seg : segments) {
      total += seg.len;
    }
    return total;
  }();

  switch (type) {
    case kReqTypeIn: {
      if ((total_data_bytes % kSectorSize) != 0u) {
        return write_status(kReqStatusIoErr);
      }
      const std::uint64_t image_offset = sector * kSectorSize;
      if (image_offset > image_.size() || total_data_bytes > (image_.size() - image_offset)) {
        return write_status(kReqStatusIoErr);
      }
      std::size_t copied = 0;
      for (const auto& seg : segments) {
        if (!seg.writable ||
            !write_guest(seg.addr, image_.data() + static_cast<std::size_t>(image_offset) + copied, seg.len)) {
          return write_status(kReqStatusIoErr);
        }
        copied += seg.len;
      }
      used_len = static_cast<std::uint32_t>(total_data_bytes + 1u);
      return write_status(kReqStatusOk);
    }
    case kReqTypeOut: {
      if ((total_data_bytes % kSectorSize) != 0u) {
        return write_status(kReqStatusIoErr);
      }
      const std::uint64_t image_offset = sector * kSectorSize;
      if (image_offset > image_.size() || total_data_bytes > (image_.size() - image_offset)) {
        return write_status(kReqStatusIoErr);
      }
      std::size_t copied = 0;
      std::vector<std::uint8_t> tmp;
      for (const auto& seg : segments) {
        if (seg.writable) {
          return write_status(kReqStatusIoErr);
        }
        tmp.resize(seg.len);
        if (!read_guest(seg.addr, tmp.data(), tmp.size())) {
          return write_status(kReqStatusIoErr);
        }
        std::memcpy(image_.data() + static_cast<std::size_t>(image_offset) + copied, tmp.data(), tmp.size());
        copied += tmp.size();
      }
      used_len = 1u;
      return write_status(kReqStatusOk);
    }
    case kReqTypeFlush:
      used_len = 1u;
      return write_status(kReqStatusOk);
    case kReqTypeGetId: {
      static constexpr std::string_view kId = "aarchvm-virtblk";
      if (segments.empty()) {
        return write_status(kReqStatusIoErr);
      }
      std::array<std::uint8_t, 20> id_bytes{};
      std::memcpy(id_bytes.data(), kId.data(), std::min(id_bytes.size(), kId.size()));
      std::size_t copied = 0;
      for (const auto& seg : segments) {
        if (!seg.writable) {
          return write_status(kReqStatusIoErr);
        }
        const std::size_t chunk = std::min<std::size_t>(seg.len, id_bytes.size() - copied);
        if (chunk == 0u) {
          break;
        }
        if (!write_guest(seg.addr, id_bytes.data() + copied, chunk)) {
          return write_status(kReqStatusIoErr);
        }
        copied += chunk;
      }
      if (copied < id_bytes.size()) {
        return write_status(kReqStatusIoErr);
      }
      used_len = static_cast<std::uint32_t>(id_bytes.size() + 1u);
      return write_status(kReqStatusOk);
    }
    default:
      used_len = 1u;
      return write_status(kReqStatusUnsupported);
  }
}

bool VirtioBlkMmio::read_descriptor(std::uint16_t index,
                                    std::uint64_t& addr,
                                    std::uint32_t& len,
                                    std::uint16_t& flags,
                                    std::uint16_t& next) const {
  if (queue_.num == 0u || index >= queue_.num) {
    return false;
  }
  std::array<std::uint8_t, 16> desc{};
  const std::uint64_t desc_addr = queue_.desc_addr + static_cast<std::uint64_t>(index) * desc.size();
  if (!read_guest(desc_addr, desc.data(), desc.size())) {
    return false;
  }
  addr = load_le<std::uint64_t>(desc.data() + 0u);
  len = load_le<std::uint32_t>(desc.data() + 8u);
  flags = load_le<std::uint16_t>(desc.data() + 12u);
  next = load_le<std::uint16_t>(desc.data() + 14u);
  return true;
}

bool VirtioBlkMmio::read_guest(std::uint64_t addr, void* dst, std::size_t size) const {
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

bool VirtioBlkMmio::write_guest(std::uint64_t addr, const void* src, std::size_t size) {
  return bus_.write_ram_buffer(addr, src, size);
}

bool VirtioBlkMmio::read_guest_u16(std::uint64_t addr, std::uint16_t& value) const {
  std::array<std::uint8_t, sizeof(value)> bytes{};
  if (!read_guest(addr, bytes.data(), bytes.size())) {
    return false;
  }
  value = load_le<std::uint16_t>(bytes.data());
  return true;
}

bool VirtioBlkMmio::write_guest_u16(std::uint64_t addr, std::uint16_t value) {
  std::array<std::uint8_t, sizeof(value)> bytes{};
  store_le<std::uint16_t>(bytes.data(), value);
  return write_guest(addr, bytes.data(), bytes.size());
}

bool VirtioBlkMmio::write_guest_u32(std::uint64_t addr, std::uint32_t value) {
  std::array<std::uint8_t, sizeof(value)> bytes{};
  store_le<std::uint32_t>(bytes.data(), value);
  return write_guest(addr, bytes.data(), bytes.size());
}

} // namespace aarchvm
