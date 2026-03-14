#pragma once

#include "aarchvm/device.hpp"

#include <cstdint>
#include <deque>
#include <iosfwd>
#include <vector>

namespace aarchvm {

class Pl050Kmi final : public Device {
public:
  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;

  void inject_rx(std::uint8_t byte);
  [[nodiscard]] bool irq_pending() const;
  void reset();

  [[nodiscard]] bool save_state(std::ostream& out) const;
  [[nodiscard]] bool load_state(std::istream& in);

private:
  enum class PendingCommand : std::uint8_t {
    None = 0,
    SetLeds,
    SetTypematic,
    SetScanSet,
    SetAllLeds,
  };

  void update_irq_state();
  void queue_response(std::uint8_t byte);
  void process_host_byte(std::uint8_t byte);
  void reset_defaults();

  std::deque<std::uint8_t> rx_fifo_;
  std::uint8_t cr_ = 0;
  std::uint8_t clkdiv_ = 0;
  std::uint8_t last_tx_byte_ = 0;
  std::uint8_t scan_set_ = 2;
  bool scanning_enabled_ = true;
  PendingCommand pending_command_ = PendingCommand::None;
};

} // namespace aarchvm
