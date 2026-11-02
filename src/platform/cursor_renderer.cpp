#include "platform/cursor_renderer.h"

#include <algorithm>
#include <cmath>

namespace cb {
namespace {

constexpr int kMinCursorPixels = 8;
constexpr int kMaxCursorPixels = 512;

int accessibility_cursor_base() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Control Panel\\Cursors",
                     L"CursorBaseSize", RRF_RT_REG_DWORD, nullptr, &value,
                     &size) != ERROR_SUCCESS) {
        return 0;
    }
    if (value == 0 || value > 1024) {
        return 0;
    }
    return static_cast<int>(value);
}

}  // namespace

CursorRenderer::~CursorRenderer() {
    clear();
}

std::vector<RenderedCursor> CursorRenderer::render(
    const std::vector<CursorThemeEntry>& entries, std::uint32_t dpi, double scale) {
    const double safe_scale =
        std::isfinite(scale) ? std::clamp(scale, 1.0, 8.0) : 1.0;
    if (!frames_.empty() && dpi_ == dpi && std::abs(scale_ - safe_scale) < 1e-6) {
        return frames_;
    }

    clear();
    dpi_ = dpi == 0 ? 96u : dpi;
    scale_ = safe_scale;

    int base = GetSystemMetricsForDpi(SM_CXCURSOR, dpi_);
    if (base <= 0) {
        base = GetSystemMetrics(SM_CXCURSOR);
    }
    if (base <= 0) {
        base = 32;
    }
    base = std::max(base, accessibility_cursor_base());

    target_px_ = std::clamp(static_cast<int>(std::lround(base * scale_)),
                            kMinCursorPixels, kMaxCursorPixels);

    for (const CursorThemeEntry& entry : entries) {
        HCURSOR handle = render_one(entry, target_px_);
        if (handle != nullptr) {
            frames_.push_back(RenderedCursor{entry.ocr_id, handle});
        }
    }

    return frames_;
}

HCURSOR CursorRenderer::render_one(const CursorThemeEntry& entry, int target_px) {
    HCURSOR result = nullptr;

    if (!entry.path.empty()) {
        // Prefer the native high-resolution frame at the requested size.
        result = reinterpret_cast<HCURSOR>(LoadImageW(
            nullptr, entry.path.c_str(), IMAGE_CURSOR, target_px, target_px,
            LR_LOADFROMFILE));

        // .ani and some legacy themes only load through LoadCursorFromFile.
        if (result == nullptr) {
            HCURSOR original = LoadCursorFromFileW(entry.path.c_str());
            if (original != nullptr) {
                result = reinterpret_cast<HCURSOR>(
                    CopyImage(original, IMAGE_CURSOR, target_px, target_px, 0));
                DestroyCursor(original);
            }
        }

        // Last attempt: load at the theme's default size, then scale.
        if (result == nullptr) {
            HCURSOR original = reinterpret_cast<HCURSOR>(LoadImageW(
                nullptr, entry.path.c_str(), IMAGE_CURSOR, 0, 0,
                LR_LOADFROMFILE | LR_DEFAULTSIZE));
            if (original != nullptr) {
                result = reinterpret_cast<HCURSOR>(
                    CopyImage(original, IMAGE_CURSOR, target_px, target_px, 0));
                DestroyCursor(original);
            }
        }
    }

    if (result == nullptr) {
        HCURSOR shared = LoadCursorW(nullptr, MAKEINTRESOURCEW(entry.ocr_id));
        if (shared == nullptr) {
            shared = LoadCursorW(nullptr, IDC_ARROW);
        }
        if (shared != nullptr) {
            result = reinterpret_cast<HCURSOR>(
                CopyImage(shared, IMAGE_CURSOR, target_px, target_px, 0));
        }
    }

    return result;
}

void CursorRenderer::clear() {
    for (RenderedCursor& frame : frames_) {
        if (frame.handle != nullptr) {
            DestroyCursor(frame.handle);
        }
    }
    frames_.clear();
    target_px_ = 0;
}

}  // namespace cb
