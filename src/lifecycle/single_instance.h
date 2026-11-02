#pragma once

#include <windows.h>

namespace cb {

class SingleInstance {
public:
    SingleInstance() = default;
    ~SingleInstance();

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    bool acquire(const wchar_t* name = L"Local\\CursorBeacon.Singleton");

    [[nodiscard]] bool acquired() const { return mutex_ != nullptr; }

private:
    HANDLE mutex_{};
};

}  // namespace cb
