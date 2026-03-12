#pragma once

#include "aarchvm/device.hpp"
#include "aarchvm/perf_types.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace aarchvm {

class PerfMailbox final : public Device {
public:
  struct Callbacks {
    std::function<void(std::uint64_t case_id, std::uint64_t arg0, std::uint64_t arg1)> begin;
    std::function<PerfResult(std::uint64_t case_id, std::uint64_t arg0, std::uint64_t arg1)> end;
    std::function<void()> request_exit;
    std::function<void()> flush_tlb;
  };

  explicit PerfMailbox(Callbacks callbacks);

  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;

private:
  std::uint64_t read_reg(std::uint64_t offset) const;
  void write_reg(std::uint64_t offset, std::uint64_t value);
  void publish_result(const PerfResult& result);

  static constexpr std::uint64_t kMagic = 0x504552464d424f58ull; // "PERFMBOX"

  Callbacks callbacks_;
  std::uint64_t case_id_ = 0;
  std::uint64_t arg0_ = 0;
  std::uint64_t arg1_ = 0;
  std::uint64_t last_status_ = 0;
  PerfResult last_result_{};
};

} // namespace aarchvm
