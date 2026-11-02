#pragma once

#include "platform/cursor_theme.h"

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace cb {

struct RenderedCursor {
    DWORD ocr_id{};
    HCURSOR handle{};
};

class CursorRenderer {
public:
    CursorRenderer() = default;
    ~CursorRenderer();

    CursorRenderer(const CursorRenderer&) = delete;
    CursorRenderer& operator=(const CursorRenderer&) = delete;

    [[nodiscard]] std::vector<RenderedCursor> render(
        const std::vector<CursorThemeEntry>& entries, std::uint32_t dpi, double scale);

    void clear();

    [[nodiscard]] int target_px() const { return target_px_; }
    [[nodiscard]] std::uint32_t dpi() const { return dpi_; }
    [[nodiscard]] double scale() const { return scale_; }

private:
    [[nodiscard]] HCURSOR render_one(const CursorThemeEntry& entry, int target_px);

    std::vector<RenderedCursor> frames_;
    std::uint32_t dpi_{};
    double scale_{1.0};
    int target_px_{};
};

}  // namespace cb
