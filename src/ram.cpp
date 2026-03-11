#include "aarchvm/ram.hpp"

#include "aarchvm/snapshot_io.hpp"

#include <cstring>

namespace aarchvm {

Ram::Ram(std::size_t size_bytes) : data_(size_bytes, 0) {}

std::uint64_t Ram::read(std::uint64_t offset, std::size_t size) {
  std::uint64_t value = 0;
  const bool ok = read_fast(offset, size, value);
  (void)ok;
  return value;
}

void Ram::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  const bool ok = write_fast(offset, value, size);
  (void)ok;
}

bool Ram::read_fast(std::uint64_t offset, std::size_t size, std::uint64_t& value) const {
  if ((size != 1u && size != 2u && size != 4u && size != 8u) || offset + size > data_.size()) {
    return false;
  }

  value = 0;
  std::memcpy(&value, data_.data() + static_cast<std::size_t>(offset), size);
  return true;
}

bool Ram::write_fast(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  if ((size != 1u && size != 2u && size != 4u && size != 8u) || offset + size > data_.size()) {
    return false;
  }

  std::memcpy(data_.data() + static_cast<std::size_t>(offset), &value, size);
  return true;
}

bool Ram::load(std::uint64_t offset, const std::vector<std::uint32_t>& words) {
  const std::size_t bytes = words.size() * sizeof(std::uint32_t);
  if (offset + bytes > data_.size()) {
    return false;
  }

  if (!words.empty()) {
    std::memcpy(data_.data() + static_cast<std::size_t>(offset), words.data(), bytes);
  }
  return true;
}

bool Ram::load_bytes(std::uint64_t offset, const std::vector<std::uint8_t>& bytes) {
  if (offset + bytes.size() > data_.size()) {
    return false;
  }
  if (!bytes.empty()) {
    std::memcpy(data_.data() + static_cast<std::size_t>(offset), bytes.data(), bytes.size());
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
