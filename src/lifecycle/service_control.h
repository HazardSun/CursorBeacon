#pragma once

#include <windows.h>

namespace cb {

inline constexpr const wchar_t* kStopEventName =
    L"Local\\CursorBeacon.StopRequest";

HANDLE create_stop_event();
bool request_stop();
bool stop_requested(HANDLE event);

}  // namespace cb
