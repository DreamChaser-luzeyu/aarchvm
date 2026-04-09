#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace aarchvm {

struct ExternalPluginConfig {
  std::string shared_object_path;
  std::string instance_name;
  std::string opaque_arg;
  std::uint64_t mmio_base = 0;
  std::uint64_t mmio_size = 0;
};

bool parse_external_plugin_spec(std::string_view spec,
                                ExternalPluginConfig& out,
                                std::string& error);

} // namespace aarchvm
