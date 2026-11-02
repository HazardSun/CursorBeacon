#include "lifecycle/single_instance.h"

namespace cb {

SingleInstance::~SingleInstance() {
    if (mutex_ != nullptr) {
        CloseHandle(mutex_);
    }
}

bool SingleInstance::acquire(const wchar_t* name) {
    if (mutex_ != nullptr) {
        return true;
    }

    mutex_ = CreateMutexW(nullptr, FALSE, name);
    if (mutex_ == nullptr) {
        return false;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex_);
        mutex_ = nullptr;
        return false;
    }
    return true;
}

}  // namespace cb
