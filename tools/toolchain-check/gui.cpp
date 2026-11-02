#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif

#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    MessageBoxW(nullptr, L"CursorBeacon GUI smoke test", L"CursorBeacon", MB_OK | MB_ICONINFORMATION);
    return 0;
}
