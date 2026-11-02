#include "lifecycle/crash_guard.h"

#include "platform/cursor_effector.h"

#include <windows.h>

#include <cstdlib>
#include <exception>

namespace cb {
namespace {

LONG WINAPI unhandled_exception(EXCEPTION_POINTERS*) {
    CursorEffector::restore_system_cursors();
    return EXCEPTION_EXECUTE_HANDLER;
}

void terminate_handler() {
    CursorEffector::restore_system_cursors();
    std::abort();
}

void at_exit_handler() {
    CursorEffector::restore_system_cursors();
}

}  // namespace

void install_crash_handlers() {
    (void)SetUnhandledExceptionFilter(&unhandled_exception);
    (void)std::set_terminate(&terminate_handler);
    (void)std::atexit(&at_exit_handler);
}

}  // namespace cb
