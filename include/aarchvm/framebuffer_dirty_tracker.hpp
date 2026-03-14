#pragma once

#include "aarchvm/ram.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace aarchvm {

class FramebufferDirtyTracker final : public RamWriteObserver {
public:
  struct RowSpan {
    std::uint32_t first = 0;
    std::uint32_t last = 0;
  };

  FramebufferDirtyTracker(std::uint32_t width,
                          std::uint32_t height,
                          std::uint32_t stride,
                          std::size_t frame_bytes)
      : width_(width),
        height_(height),
        stride_(stride),
        frame_bytes_(frame_bytes),
        dirty_rows_((static_cast<std::size_t>(height) + 63u) / 64u, 0) {}

  void on_ram_write(std::uint64_t offset, std::size_t size) override {
    if (size == 0 || stride_ == 0 || height_ == 0) {
      return;
    }
    if (offset >= frame_bytes_) {
      return;
    }

    const std::uint64_t clamped_last = std::min<std::uint64_t>(frame_bytes_ - 1u,
                                                               offset + static_cast<std::uint64_t>(size) - 1u);
    const std::uint32_t first_row = static_cast<std::uint32_t>(offset / stride_);
    const std::uint32_t last_row = static_cast<std::uint32_t>(clamped_last / stride_);
    mark_rows(first_row, last_row);
  }

  void mark_all_dirty() {
    if (height_ == 0) {
      return;
    }
    mark_rows(0, height_ - 1u);
  }

  [[nodiscard]] bool has_dirty() const {
    return dirty_generation_ != consumed_generation_;
  }

  void consume_dirty_rows(std::vector<RowSpan>& out) {
    out.clear();
    if (!has_dirty()) {
      return;
    }

    bool in_run = false;
    std::uint32_t run_first = 0;
    for (std::uint32_t row = 0; row < height_; ++row) {
      if (row_is_dirty(row)) {
        if (!in_run) {
          run_first = row;
          in_run = true;
        }
        continue;
      }
      if (in_run) {
        out.push_back(RowSpan{.first = run_first, .last = row - 1u});
        in_run = false;
      }
    }
    if (in_run) {
      out.push_back(RowSpan{.first = run_first, .last = height_ - 1u});
    }

    std::fill(dirty_rows_.begin(), dirty_rows_.end(), 0);
    consumed_generation_ = dirty_generation_;
  }

private:
  void mark_rows(std::uint32_t first_row, std::uint32_t last_row) {
    if (first_row >= height_) {
      return;
    }
    last_row = std::min(last_row, height_ - 1u);
    for (std::uint32_t row = first_row; row <= last_row; ++row) {
      dirty_rows_[row / 64u] |= (1ull << (row % 64u));
    }
    ++dirty_generation_;
  }

  [[nodiscard]] bool row_is_dirty(std::uint32_t row) const {
    return (dirty_rows_[row / 64u] & (1ull << (row % 64u))) != 0;
  }

  std::uint32_t width_ = 0;
  std::uint32_t height_ = 0;
  std::uint32_t stride_ = 0;
  std::size_t frame_bytes_ = 0;
  std::vector<std::uint64_t> dirty_rows_;
  std::uint64_t dirty_generation_ = 0;
  std::uint64_t consumed_generation_ = 0;
};

} // namespace aarchvm
