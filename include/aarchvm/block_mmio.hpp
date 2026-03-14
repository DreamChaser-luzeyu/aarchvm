#pragma once

#include "aarchvm/device.hpp"

#include <cstdint>
#include <iosfwd>
#include <vector>

namespace aarchvm {

class Bus;

class BlockMmio final : public Device {
public:
  explicit BlockMmio(Bus& bus);

  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;

  void set_image(std::vector<std::uint8_t> bytes);
  [[nodiscard]] std::uint64_t num_blocks() const;
  [[nodiscard]] std::uint32_t block_size() const { return kBlockSize; }

  [[nodiscard]] bool save_state(std::ostream& out) const;
  [[nodiscard]] bool load_state(std::istream& in);

private:
  enum class Status : std::uint32_t {
    Ok = 0,
    BadCommand = 1,
    Range = 2,
    Buffer = 3,
    Count = 4,
  };

  void execute_command(std::uint32_t command);
  [[nodiscard]] bool transfer_from_image();
  [[nodiscard]] bool transfer_to_image();
  [[nodiscard]] bool valid_transfer_range();
  void reset_registers();

  static constexpr std::uint32_t kMagic = 0x41424c4bu;
  static constexpr std::uint32_t kVersion = 1u;
  static constexpr std::uint32_t kBlockSize = 512u;
  static constexpr std::uint32_t kFeatureWrite = 1u << 0;

  Bus& bus_;
  std::vector<std::uint8_t> image_;
  std::uint64_t buffer_addr_ = 0;
  std::uint64_t lba_ = 0;
  std::uint32_t count_ = 0;
  Status status_ = Status::Ok;
};

} // namespace aarchvm
