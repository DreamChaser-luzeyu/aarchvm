#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

namespace aarchvm::snapshot_io {

template <typename T>
concept TriviallySerializable = std::is_trivially_copyable_v<T>;

template <TriviallySerializable T>
inline bool write(std::ostream& out, const T& value) {
  out.write(reinterpret_cast<const char*>(&value), sizeof(value));
  return static_cast<bool>(out);
}

template <TriviallySerializable T>
inline bool read(std::istream& in, T& value) {
  in.read(reinterpret_cast<char*>(&value), sizeof(value));
  return static_cast<bool>(in);
}

inline bool write_bool(std::ostream& out, bool value) {
  const std::uint8_t v = value ? 1u : 0u;
  return write(out, v);
}

inline bool read_bool(std::istream& in, bool& value) {
  std::uint8_t v = 0;
  if (!read(in, v)) {
    return false;
  }
  value = v != 0;
  return true;
}

template <typename T, std::size_t N>
inline bool write_array(std::ostream& out, const std::array<T, N>& values) {
  for (const auto& value : values) {
    if constexpr (std::is_same_v<T, bool>) {
      if (!write_bool(out, value)) {
        return false;
      }
    } else {
      if (!write(out, value)) {
        return false;
      }
    }
  }
  return true;
}

template <typename T, std::size_t N>
inline bool read_array(std::istream& in, std::array<T, N>& values) {
  for (auto& value : values) {
    if constexpr (std::is_same_v<T, bool>) {
      if (!read_bool(in, value)) {
        return false;
      }
    } else {
      if (!read(in, value)) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
inline bool write_vector(std::ostream& out, const std::vector<T>& values) {
  const std::uint64_t size = static_cast<std::uint64_t>(values.size());
  if (!write(out, size)) {
    return false;
  }
  if constexpr (std::is_same_v<T, bool>) {
    for (const bool value : values) {
      if (!write_bool(out, value)) {
        return false;
      }
    }
    return true;
  } else if constexpr (TriviallySerializable<T>) {
    if (values.empty()) {
      return true;
    }
    out.write(reinterpret_cast<const char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(T)));
    return static_cast<bool>(out);
  } else {
    return false;
  }
}

template <typename T>
inline bool read_vector(std::istream& in, std::vector<T>& values) {
  std::uint64_t size = 0;
  if (!read(in, size)) {
    return false;
  }
  if (size > (1ull << 31)) {
    return false;
  }
  values.resize(static_cast<std::size_t>(size));
  if constexpr (std::is_same_v<T, bool>) {
    for (bool& value : values) {
      if (!read_bool(in, value)) {
        return false;
      }
    }
    return true;
  } else if constexpr (TriviallySerializable<T>) {
    if (values.empty()) {
      return true;
    }
    in.read(reinterpret_cast<char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(T)));
    return static_cast<bool>(in);
  } else {
    return false;
  }
}

inline bool write_string(std::ostream& out, const std::string& value) {
  const std::uint64_t size = static_cast<std::uint64_t>(value.size());
  if (!write(out, size)) {
    return false;
  }
  out.write(value.data(), static_cast<std::streamsize>(value.size()));
  return static_cast<bool>(out);
}

inline bool read_string(std::istream& in, std::string& value) {
  std::uint64_t size = 0;
  if (!read(in, size)) {
    return false;
  }
  if (size > (1ull << 20)) {
    return false;
  }
  value.resize(static_cast<std::size_t>(size));
  if (size == 0) {
    return true;
  }
  in.read(value.data(), static_cast<std::streamsize>(size));
  return static_cast<bool>(in);
}

} // namespace aarchvm::snapshot_io
