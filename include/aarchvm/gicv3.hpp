#pragma once

#include "aarchvm/device.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace aarchvm {

class GicV3 final : public Device {
public:
  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;

  void set_pending(std::uint32_t intid);
  [[nodiscard]] bool has_pending() const;
  [[nodiscard]] std::optional<std::uint32_t> acknowledge();
  void eoi(std::uint32_t intid);

private:
  std::array<bool, 1024> pending_{};
};

} // namespace aarchvm
