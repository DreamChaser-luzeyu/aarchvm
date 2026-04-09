#pragma once

#include "aarchvm/plugin_config.hpp"

namespace aarchvm {

int run_plugin_child(int fd, const ExternalPluginConfig& config);

} // namespace aarchvm
