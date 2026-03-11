#pragma once

#include <cstddef>
#include <cstdint>

namespace aarchvm {

class Ram;

class BusFastPath {
public:
  BusFastPath(Ram& boot_ram, Ram& sdram);

  [[nodiscard]] bool read(std::uint64_t addr, std::size_t size, std::uint64_t& value) const;
  [[nodiscard]] bool write(std::uint64_t addr, std::uint64_t value, std::size_t size) const;

private:
  Ram* boot_ram_ = nullptr;
  Ram* sdram_ = nullptr;
};

} // namespace aarchvm
