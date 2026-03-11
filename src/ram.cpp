#include "aarchvm/ram.hpp"

#include "aarchvm/snapshot_io.hpp"

namespace aarchvm {

Ram::Ram(std::size_t size_bytes) : data_(size_bytes, 0) {}

std::uint64_t Ram::read(std::uint64_t offset, std::size_t size) {
  if (offset + size > data_.size()) {
    return 0;
  }

  std::uint64_t value = 0;
  for (std::size_t i = 0; i < size; ++i) {
    value |= static_cast<std::uint64_t>(data_[static_cast<std::size_t>(offset + i)]) << (8 * i);
  }
  return value;
}

void Ram::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  if (offset + size > data_.size()) {
    return;
  }

  for (std::size_t i = 0; i < size; ++i) {
    data_[static_cast<std::size_t>(offset + i)] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFu);
  }
}

bool Ram::load(std::uint64_t offset, const std::vector<std::uint32_t>& words) {
  const std::size_t bytes = words.size() * sizeof(std::uint32_t);
  if (offset + bytes > data_.size()) {
    return false;
  }

  for (std::size_t i = 0; i < words.size(); ++i) {
    write(offset + i * 4, words[i], 4);
  }
  return true;
}

bool Ram::load_bytes(std::uint64_t offset, const std::vector<std::uint8_t>& bytes) {
  if (offset + bytes.size() > data_.size()) {
    return false;
  }
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    data_[offset + i] = bytes[i];
  }
  return true;
}

bool Ram::save_state(std::ostream& out) const {
  return snapshot_io::write_vector(out, data_);
}

bool Ram::load_state(std::istream& in) {
  std::vector<std::uint8_t> data;
  if (!snapshot_io::read_vector(in, data)) {
    return false;
  }
  if (data.size() != data_.size()) {
    return false;
  }
  data_ = std::move(data);
  return true;
}

} // namespace aarchvm
