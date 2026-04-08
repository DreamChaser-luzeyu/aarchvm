#include "aarchvm/block_mmio.hpp"

#include "aarchvm/bus.hpp"
#include "aarchvm/snapshot_io.hpp"

#include <algorithm>
#include <cstring>

namespace aarchvm {

namespace {

constexpr std::uint64_t kRegMagic = 0x00;
constexpr std::uint64_t kRegVersion = 0x04;
constexpr std::uint64_t kRegBlockSize = 0x08;
constexpr std::uint64_t kRegFeatures = 0x0C;
constexpr std::uint64_t kRegNumBlocks = 0x10;
constexpr std::uint64_t kRegBufferAddr = 0x18;
constexpr std::uint64_t kRegLba = 0x20;
constexpr std::uint64_t kRegCount = 0x28;
constexpr std::uint64_t kRegStatus = 0x2C;
constexpr std::uint64_t kRegCommand = 0x30;

bool access_matches(std::uint64_t offset,
                    std::size_t size,
                    std::uint64_t reg_offset,
                    std::size_t reg_size) {
  return offset == reg_offset && size == reg_size;
}

} // namespace

BlockMmio::BlockMmio(Bus& bus) : bus_(bus) {}

std::uint64_t BlockMmio::read(std::uint64_t offset, std::size_t size) {
  if (access_matches(offset, size, kRegMagic, sizeof(std::uint32_t))) {
    return kMagic;
  }
  if (access_matches(offset, size, kRegVersion, sizeof(std::uint32_t))) {
    return kVersion;
  }
  if (access_matches(offset, size, kRegBlockSize, sizeof(std::uint32_t))) {
    return kBlockSize;
  }
  if (access_matches(offset, size, kRegFeatures, sizeof(std::uint32_t))) {
    return kFeatureWrite;
  }
  if (access_matches(offset, size, kRegNumBlocks, sizeof(std::uint64_t))) {
    return num_blocks();
  }
  if (access_matches(offset, size, kRegBufferAddr, sizeof(std::uint64_t))) {
    return buffer_addr_;
  }
  if (access_matches(offset, size, kRegLba, sizeof(std::uint64_t))) {
    return lba_;
  }
  if (access_matches(offset, size, kRegCount, sizeof(std::uint32_t))) {
    return count_;
  }
  if (access_matches(offset, size, kRegStatus, sizeof(std::uint32_t))) {
    return static_cast<std::uint32_t>(status_);
  }
  return 0;
}

void BlockMmio::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  if (access_matches(offset, size, kRegBufferAddr, sizeof(std::uint64_t))) {
    buffer_addr_ = value;
    return;
  }
  if (access_matches(offset, size, kRegLba, sizeof(std::uint64_t))) {
    lba_ = value;
    return;
  }
  if (access_matches(offset, size, kRegCount, sizeof(std::uint32_t))) {
    count_ = static_cast<std::uint32_t>(value);
    return;
  }
  if (access_matches(offset, size, kRegStatus, sizeof(std::uint32_t))) {
    status_ = Status::Ok;
    return;
  }
  if (access_matches(offset, size, kRegCommand, sizeof(std::uint32_t))) {
    execute_command(static_cast<std::uint32_t>(value));
  }
}

void BlockMmio::set_image(std::vector<std::uint8_t> bytes) {
  const std::size_t padded_size =
      ((bytes.size() + static_cast<std::size_t>(kBlockSize) - 1u) / static_cast<std::size_t>(kBlockSize)) *
      static_cast<std::size_t>(kBlockSize);
  bytes.resize(padded_size, 0);
  image_ = std::move(bytes);
  reset_registers();
}

std::uint64_t BlockMmio::num_blocks() const {
  return static_cast<std::uint64_t>(image_.size() / kBlockSize);
}

bool BlockMmio::save_state(std::ostream& out) const {
  return snapshot_io::write_vector(out, image_) &&
         snapshot_io::write(out, buffer_addr_) &&
         snapshot_io::write(out, lba_) &&
         snapshot_io::write(out, count_) &&
         snapshot_io::write(out, static_cast<std::uint32_t>(status_));
}

bool BlockMmio::load_state(std::istream& in) {
  std::uint32_t status = 0;
  if (!snapshot_io::read_vector(in, image_) ||
      !snapshot_io::read(in, buffer_addr_) ||
      !snapshot_io::read(in, lba_) ||
      !snapshot_io::read(in, count_) ||
      !snapshot_io::read(in, status)) {
    return false;
  }
  if ((image_.size() % kBlockSize) != 0u || status > static_cast<std::uint32_t>(Status::Count)) {
    return false;
  }
  status_ = static_cast<Status>(status);
  return true;
}

void BlockMmio::execute_command(std::uint32_t command) {
  status_ = Status::Ok;
  switch (command) {
    case 1u:
      if (!transfer_from_image()) {
        if (status_ == Status::Ok) {
          status_ = Status::Buffer;
        }
      }
      break;
    case 2u:
      if (!transfer_to_image()) {
        if (status_ == Status::Ok) {
          status_ = Status::Buffer;
        }
      }
      break;
    default:
      status_ = Status::BadCommand;
      break;
  }
}

bool BlockMmio::transfer_from_image() {
  if (!valid_transfer_range()) {
    return false;
  }
  const std::size_t byte_count = static_cast<std::size_t>(count_) * static_cast<std::size_t>(kBlockSize);
  const std::size_t image_off = static_cast<std::size_t>(lba_) * static_cast<std::size_t>(kBlockSize);
  if (!bus_.write_ram_buffer(buffer_addr_, image_.data() + image_off, byte_count)) {
    status_ = Status::Buffer;
    return false;
  }
  return true;
}

bool BlockMmio::transfer_to_image() {
  if (!valid_transfer_range()) {
    return false;
  }
  const std::size_t byte_count = static_cast<std::size_t>(count_) * static_cast<std::size_t>(kBlockSize);
  const std::uint8_t* src = bus_.ram_ptr(buffer_addr_, byte_count);
  if (src == nullptr) {
    status_ = Status::Buffer;
    return false;
  }
  const std::size_t image_off = static_cast<std::size_t>(lba_) * static_cast<std::size_t>(kBlockSize);
  std::memcpy(image_.data() + image_off, src, byte_count);
  return true;
}

bool BlockMmio::valid_transfer_range() {
  if (count_ == 0u) {
    status_ = Status::Count;
    return false;
  }
  if (lba_ >= num_blocks() || static_cast<std::uint64_t>(count_) > (num_blocks() - lba_)) {
    status_ = Status::Range;
    return false;
  }
  return true;
}

void BlockMmio::reset_registers() {
  buffer_addr_ = 0;
  lba_ = 0;
  count_ = 0;
  status_ = Status::Ok;
}

} // namespace aarchvm
