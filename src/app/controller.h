#pragma once

#include "config/config.h"

#include <atomic>

namespace cb {

int run_controller(const Config& config, bool apply_effects,
                   std::atomic<bool>& stop, int stats_seconds,
                   bool print_stats);

}  // namespace cb
