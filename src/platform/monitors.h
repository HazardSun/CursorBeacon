#pragma once

#include "core/types.h"

#include <mutex>
#include <optional>
#include <vector>

namespace cb {

class MonitorService {
public:
    MonitorService();

    void refresh();

    [[nodiscard]] std::vector<MonitorInfo> snapshot() const;
    [[nodiscard]] std::optional<MonitorInfo> for_point(Point point) const;
    [[nodiscard]] uint32_t dpi_for_point(Point point) const;
    [[nodiscard]] bool find_by_handle(uintptr_t handle, MonitorInfo& out) const;

private:
    mutable std::mutex mutex_;
    std::vector<MonitorInfo> monitors_;
};

}  // namespace cb
