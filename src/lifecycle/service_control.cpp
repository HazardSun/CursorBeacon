#include "lifecycle/service_control.h"

namespace cb {

HANDLE create_stop_event() {
    return CreateEventW(nullptr, FALSE, FALSE, kStopEventName);
}

bool request_stop() {
    HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, kStopEventName);
    if (event == nullptr) {
        return false;
    }
    const bool ok = SetEvent(event) != FALSE;
    CloseHandle(event);
    return ok;
}

bool stop_requested(HANDLE event) {
    if (event == nullptr) {
        return false;
    }
    return WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
}

}  // namespace cb
