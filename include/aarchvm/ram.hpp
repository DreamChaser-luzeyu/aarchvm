#pragma once

#include "aarchvm/device.hpp"

#include <cstdint>
#include <iosfwd>
#include <vector>

namespace aarchvm {

class Ram final : public Device {
public:
  explicit Ram(std::size_t size_bytes);

  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;

  [[nodiscard]] std::size_t size() const { return data_.size(); }

  bool load(std::uint64_t offset, const std::vector<std::uint32_t>& words);
  bool load_bytes(std::uint64_t offset, const std::vector<std::uint8_t>& bytes);
  [[nodiscard]] bool save_state(std::ostream& out) const;
  [[nodiscard]] bool load_state(std::istream& in);

private:
  std::vector<std::uint8_t> data_;
};

} // namespace aarchvm
