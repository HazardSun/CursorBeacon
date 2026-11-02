#include "platform/cursor_effector.h"

#include <array>
#include <cstdio>
#include <cwchar>

namespace cb {
namespace {

std::wstring local_app_data() {
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length =
        GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(),
                                static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return L".";
    }
    return std::wstring(buffer.data(), length);
}

std::wstring application_directory() {
    const std::wstring base = local_app_data();
    const std::wstring directory = base + L"\\CursorBeacon";
    CreateDirectoryW(directory.c_str(), nullptr);
    return directory;
}

}  // namespace

CursorEffector::~CursorEffector() {
    if (applied_) {
        try {
            restore();
        } catch (...) {
            CursorEffector::restore_system_cursors();
        }
    }
}

bool CursorEffector::apply(const std::vector<RenderedCursor>& frames, double scale) {
    if (frames.empty()) {
        last_error_ = L"no rendered cursor frames";
        return false;
    }

    if (!applied_) {
        write_dirty_flag();
    }

    int applied_count = 0;
    std::wstring errors;

    for (const RenderedCursor& frame : frames) {
        HCURSOR copy = reinterpret_cast<HCURSOR>(
            CopyIcon(reinterpret_cast<HICON>(frame.handle)));
        if (copy == nullptr) {
            continue;
        }

        if (!SetSystemCursor(copy, frame.ocr_id)) {
            const DWORD error = GetLastError();
            DestroyCursor(copy);
            wchar_t buffer[96]{};
            const int written =
                swprintf(buffer, 96, L"SetSystemCursor(%lu) failed: %lu; ",
                         static_cast<unsigned long>(frame.ocr_id),
                         static_cast<unsigned long>(error));
            if (written > 0) {
                errors += buffer;
            }
            continue;
        }
        ++applied_count;
    }

    if (applied_count == 0) {
        last_error_ = errors.empty() ? L"all cursor replacements failed" : errors;
        restore();
        return false;
    }

    applied_ = true;
    scale_ = scale;
    last_error_.clear();
    return true;
}

bool CursorEffector::restore() {
    const bool ok = restore_system_cursors();
    if (!ok) {
        last_error_ = L"SystemParametersInfo(SPI_SETCURSORS) failed";
        return false;
    }

    clear_dirty_flag();
    applied_ = false;
    scale_ = 1.0;
    return true;
}

bool CursorEffector::restore_system_cursors() {
    return SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr, SPIF_SENDCHANGE) != FALSE;
}

std::wstring CursorEffector::dirty_flag_path() {
    return application_directory() + L"\\restore.needed";
}

bool CursorEffector::recover_if_needed() {
    const std::wstring path = dirty_flag_path();
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    if (!restore_system_cursors()) {
        return false;
    }
    DeleteFileW(path.c_str());
    return true;
}

void CursorEffector::write_dirty_flag() {
    const std::wstring path = dirty_flag_path();
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char buffer[96]{};
    const int length = std::snprintf(buffer, sizeof(buffer), "pid=%lu\r\n",
                                     static_cast<unsigned long>(GetCurrentProcessId()));
    if (length > 0) {
        DWORD written = 0;
        WriteFile(file, buffer, static_cast<DWORD>(length), &written, nullptr);
    }
    CloseHandle(file);
}

void CursorEffector::clear_dirty_flag() {
    DeleteFileW(dirty_flag_path().c_str());
}

}  // namespace cb
