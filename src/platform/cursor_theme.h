#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace cb {

struct CursorThemeEntry {
    std::wstring registry_name;
    std::wstring path;
    DWORD ocr_id{};
};

class CursorTheme {
public:
    void reload();

    [[nodiscard]] const std::vector<CursorThemeEntry>& entries() const { return entries_; }
    [[nodiscard]] std::vector<CursorThemeEntry> selected(
        const std::vector<std::wstring>& names) const;

    [[nodiscard]] static std::vector<std::wstring> default_cursor_names();

private:
    std::vector<CursorThemeEntry> entries_;
};

}  // namespace cb
