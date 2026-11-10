#include "util/time.h"

#include <windows.h>

namespace cb {

int64_t now_us() {
    static const LARGE_INTEGER frequency = [] {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return value;
    }();

    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return qpc_to_us(counter.QuadPart, frequency.QuadPart);
}

}  // namespace cb
