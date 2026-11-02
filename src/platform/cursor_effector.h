#pragma once

#include "platform/cursor_renderer.h"

#include <windows.h>

#include <string>
#include <vector>

namespace cb {

class CursorEffector {
public:
    CursorEffector() = default;
    ~CursorEffector();

    CursorEffector(const CursorEffector&) = delete;
    CursorEffector& operator=(const CursorEffector&) = delete;

    bool apply(const std::vector<RenderedCursor>& frames, double scale);
    bool restore();

    [[nodiscard]] bool applied() const { return applied_; }
    [[nodiscard]] double scale() const { return scale_; }
    [[nodiscard]] const std::wstring& last_error() const { return last_error_; }

    static bool restore_system_cursors();
    static std::wstring dirty_flag_path();
    static bool recover_if_needed();

private:
    static void write_dirty_flag();
    static void clear_dirty_flag();

    bool applied_{};
    double scale_{1.0};
    std::wstring last_error_;
};

}  // namespace cb
