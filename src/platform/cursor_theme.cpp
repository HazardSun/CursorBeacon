#include "platform/cursor_theme.h"

#include <algorithm>
#include <array>
#include <cwchar>

namespace cb {
namespace {

struct CursorMapping {
    const wchar_t* registry_name;
    DWORD ocr_id;
};

// Numeric OCR_* values avoid requiring the OEMRESOURCE macro before windows.h.
constexpr std::array<CursorMapping, 14> kMappings{{
    {L"Arrow", 32512},        // OCR_NORMAL
    {L"IBeam", 32513},        // OCR_IBEAM
    {L"Wait", 32514},         // OCR_WAIT
    {L"Crosshair", 32515},    // OCR_CROSS
    {L"UpArrow", 32516},      // OCR_UP
    {L"SizeNWSE", 32642},     // OCR_SIZENWSE
    {L"SizeNESW", 32643},     // OCR_SIZENESW
    {L"SizeWE", 32644},       // OCR_SIZEWE
    {L"SizeNS", 32645},       // OCR_SIZENS
    {L"SizeAll", 32646},      // OCR_SIZEALL
    {L"No", 32648},           // OCR_NO
    {L"Hand", 32649},         // OCR_HAND
    {L"AppStarting", 32650},  // OCR_APPSTARTING
    {L"Help", 32651},         // OCR_HELP
}};

std::wstring read_registry_string(HKEY key, const wchar_t* value_name) {
    constexpr DWORD kMaxRegistryStringBytes = 64 * 1024;

    DWORD type = 0;
    DWORD bytes = 0;
    if (RegQueryValueExW(key, value_name, nullptr, &type, nullptr, &bytes) !=
        ERROR_SUCCESS) {
        return {};
    }
    if ((type != REG_SZ && type != REG_EXPAND_SZ) || bytes == 0 ||
        bytes > kMaxRegistryStringBytes) {
        return {};
    }

    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    const DWORD capacity = bytes;
    if (RegQueryValueExW(key, value_name, nullptr, &type,
                         reinterpret_cast<LPBYTE>(value.data()), &bytes) !=
        ERROR_SUCCESS) {
        return {};
    }
    if (bytes > capacity) {
        return {};
    }
    if (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }

    if (type == REG_EXPAND_SZ && !value.empty()) {
        const DWORD required = ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);
        if (required > 1 && required <= kMaxRegistryStringBytes / sizeof(wchar_t)) {
            std::wstring expanded(required, L'\0');
            const DWORD written =
                ExpandEnvironmentStringsW(value.c_str(), expanded.data(), required);
            if (written > 0 && written <= required) {
                expanded.resize(written - 1);
                return expanded;
            }
        }
    }

    return value;
}

bool name_matches(const std::wstring& value, const std::wstring& wanted) {
    return _wcsicmp(value.c_str(), wanted.c_str()) == 0;
}

}  // namespace

void CursorTheme::reload() {
    entries_.clear();
    entries_.reserve(kMappings.size());

    HKEY key = nullptr;
    const bool opened =
        RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Cursors", 0, KEY_READ,
                      &key) == ERROR_SUCCESS;

    for (const CursorMapping& mapping : kMappings) {
        CursorThemeEntry entry;
        entry.registry_name = mapping.registry_name;
        entry.ocr_id = mapping.ocr_id;
        if (opened) {
            entry.path = read_registry_string(key, mapping.registry_name);
        }
        entries_.push_back(std::move(entry));
    }

    if (opened) {
        RegCloseKey(key);
    }
}

std::vector<CursorThemeEntry> CursorTheme::selected(
    const std::vector<std::wstring>& names) const {
    const bool all =
        std::any_of(names.begin(), names.end(),
                    [](const std::wstring& name) { return name_matches(name, L"all"); });

    std::vector<CursorThemeEntry> result;
    for (const CursorThemeEntry& entry : entries_) {
        if (all) {
            result.push_back(entry);
            continue;
        }
        const bool wanted =
            std::any_of(names.begin(), names.end(), [&](const std::wstring& name) {
                return name_matches(entry.registry_name, name);
            });
        if (wanted) {
            result.push_back(entry);
        }
    }
    return result;
}

std::vector<std::wstring> CursorTheme::default_cursor_names() {
    return {L"Arrow", L"IBeam", L"Hand", L"Crosshair"};
}

}  // namespace cb
