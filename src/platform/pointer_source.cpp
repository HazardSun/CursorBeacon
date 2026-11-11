#include "platform/pointer_source.h"

#include "platform/monitors.h"
#include "util/time.h"

#include <algorithm>
#include <future>
#include <limits>
#include <memory>
#include <utility>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace cb {
namespace {

constexpr wchar_t kWindowClass[] = L"CursorBeaconPointerSource";
constexpr std::int64_t kMinSampleIntervalUs = 4'000;
constexpr std::int64_t kPollIntervalUs = 8'000;

}  // namespace

SampleQueue::SampleQueue() {
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
}

SampleQueue::~SampleQueue() {
    if (event_ != nullptr) {
        CloseHandle(event_);
    }
}

void SampleQueue::push(const Sample& sample) {
    {
        std::lock_guard lock(mutex_);
        if (count_ == buffer_.size()) {
            head_ = (head_ + 1) % buffer_.size();
            --count_;
        }
        const std::size_t index = (head_ + count_) % buffer_.size();
        buffer_[index] = sample;
        ++count_;
    }
    if (event_ != nullptr) {
        SetEvent(event_);
    }
}

bool SampleQueue::pop(Sample& sample) {
    std::lock_guard lock(mutex_);
    if (count_ == 0) {
        return false;
    }
    sample = buffer_[head_];
    head_ = (head_ + 1) % buffer_.size();
    --count_;
    return true;
}

bool SampleQueue::wait(int timeout_ms) {
    if (event_ == nullptr) {
        Sleep(static_cast<DWORD>(std::max(0, timeout_ms)));
        return false;
    }
    return WaitForSingleObject(event_, static_cast<DWORD>(std::max(0, timeout_ms))) ==
           WAIT_OBJECT_0;
}

std::size_t SampleQueue::size() const {
    std::lock_guard lock(mutex_);
    return count_;
}

PointerSource::PointerSource(SampleQueue& queue, MonitorService& monitors)
    : queue_(queue), monitors_(monitors) {}

PointerSource::~PointerSource() {
    stop();
}

bool PointerSource::start() {
    if (running_.exchange(true)) {
        return true;
    }

    if (thread_.joinable()) {
        thread_.join();
    }

    stopping_ = false;
    poll_stop_ = false;

    auto promise = std::make_shared<std::promise<bool>>();
    std::future<bool> future = promise->get_future();
    thread_ = std::thread([this, promise] {
        const bool initialized = initialize();
        promise->set_value(initialized);
        if (initialized) {
            message_loop();
        }
        running_ = false;
    });

    return future.get();
}

void PointerSource::stop() {
    stopping_ = true;
    polling_until_us_ = 0;

    const HWND hwnd = hwnd_.load();
    if (hwnd != nullptr) {
        PostMessageW(hwnd, kStopMessage, 0, 0);
    }

    stop_polling();
    if (thread_.joinable()) {
        thread_.join();
    }
    running_ = false;
}

PointerSource::Mode PointerSource::mode() const {
    if (raw_available_.load() && !poll_running_.load()) {
        return Mode::RawInput;
    }
    return Mode::Polling;
}

bool PointerSource::initialize() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &PointerSource::wnd_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kWindowClass;

    if (RegisterClassExW(&window_class) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    const HWND hwnd = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0,
                                      HWND_MESSAGE, nullptr, instance, this);
    if (hwnd == nullptr) {
        return false;
    }
    hwnd_ = hwnd;

    RAWINPUTDEVICE device{};
    device.usUsagePage = 0x01;  // Generic Desktop
    device.usUsage = 0x02;      // Mouse
    device.dwFlags = RIDEV_INPUTSINK;
    device.hwndTarget = hwnd;
    raw_available_ =
        RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;

    if (SetTimer(hwnd, kWatchdogTimer, 100, nullptr) == 0) {
        raw_available_ = false;
    }
    SetTimer(hwnd, kHealthTimer, 5'000, nullptr);

    if (!raw_available_.load()) {
        ensure_polling(std::numeric_limits<std::int64_t>::max() / 4);
    }

    return true;
}

void PointerSource::refresh_raw_input() {
    const HWND hwnd = hwnd_.load();
    if (hwnd == nullptr) {
        return;
    }

    RAWINPUTDEVICE device{};
    device.usUsagePage = 0x01;
    device.usUsage = 0x02;
    device.dwFlags = RIDEV_INPUTSINK;
    device.hwndTarget = hwnd;

    const bool ok = RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
    raw_available_ = ok;

    if (!ok) {
        ensure_polling(std::numeric_limits<std::int64_t>::max() / 4);
        return;
    }

    if (SetTimer(hwnd, kWatchdogTimer, 100, nullptr) == 0) {
        raw_available_ = false;
        ensure_polling(std::numeric_limits<std::int64_t>::max() / 4);
        return;
    }

    // If raw events are flowing again, let an infinite polling fallback exit.
    if (now_us() - last_raw_event_us_.load() < 1'000'000) {
        polling_until_us_ = 0;
    }
}

void PointerSource::message_loop() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    KillTimer(hwnd_.load(), kWatchdogTimer);

    RAWINPUTDEVICE remove{};
    remove.usUsagePage = 0x01;
    remove.usUsage = 0x02;
    remove.dwFlags = RIDEV_REMOVE;
    remove.hwndTarget = nullptr;
    RegisterRawInputDevices(&remove, 1, sizeof(remove));

    hwnd_ = nullptr;
    stop_polling();
}

LRESULT CALLBACK PointerSource::wnd_proc(HWND hwnd, UINT message, WPARAM wparam,
                                         LPARAM lparam) {
    PointerSource* self = reinterpret_cast<PointerSource*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = reinterpret_cast<PointerSource*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self != nullptr) {
        return self->handle_message(hwnd, message, wparam, lparam);
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT PointerSource::handle_message(HWND hwnd, UINT message, WPARAM wparam,
                                      LPARAM lparam) {
    switch (message) {
        case WM_INPUT:
            handle_raw_input(lparam);
            return 0;
        case WM_TIMER:
            if (wparam == kWatchdogTimer) {
                handle_watchdog();
            } else if (wparam == kHealthTimer) {
                refresh_raw_input();
            }
            return 0;
        case kStopMessage:
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

void PointerSource::handle_raw_input(LPARAM lparam) {
    RAWINPUT input{};
    UINT size = sizeof(input);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, &input,
                        &size, sizeof(RAWINPUTHEADER)) != size) {
        return;
    }
    if (input.header.dwType != RIM_TYPEMOUSE) {
        return;
    }

    last_raw_event_us_ = now_us();
    raw_events_.fetch_add(1, std::memory_order_relaxed);

    // While the polling fallback is active, use it as the single sample
    // producer. Mixing both streams can interleave timestamps and generate
    // false velocity spikes.
    if (!poll_running_.load()) {
        sample_once(false);
    }
}

void PointerSource::handle_watchdog() {
    watchdog_checks_.fetch_add(1, std::memory_order_relaxed);

    if (!raw_available_.load()) {
        return;
    }

    const std::int64_t now = now_us();
    if (now - last_raw_event_us_.load() < 500'000) {
        return;
    }

    POINT point{};
    if (!GetPhysicalCursorPos(&point) && !GetCursorPos(&point)) {
        return;
    }

    if (point.x != last_x_.load() || point.y != last_y_.load()) {
        ensure_polling(3'000'000);
    }
}

void PointerSource::sample_once(bool polled) {
    const std::int64_t now = now_us();
    if (now - last_sample_us_.load() < kMinSampleIntervalUs) {
        return;
    }

    POINT point{};
    if (!GetPhysicalCursorPos(&point) && !GetCursorPos(&point)) {
        return;
    }

    std::uint8_t buttons = 0;
    if ((static_cast<unsigned short>(GetAsyncKeyState(VK_LBUTTON)) & 0x8000u) != 0) {
        buttons |= kButtonLeft;
    }
    if ((static_cast<unsigned short>(GetAsyncKeyState(VK_RBUTTON)) & 0x8000u) != 0) {
        buttons |= kButtonRight;
    }
    if ((static_cast<unsigned short>(GetAsyncKeyState(VK_MBUTTON)) & 0x8000u) != 0) {
        buttons |= kButtonMiddle;
    }

    const std::uint32_t dpi = monitors_.dpi_for_point(Point{point.x, point.y});

    last_sample_us_ = now;
    last_x_ = point.x;
    last_y_ = point.y;

    queue_.push(Sample{now, point.x, point.y, buttons, dpi});
    if (polled) {
        polled_samples_.fetch_add(1, std::memory_order_relaxed);
    }
}

void PointerSource::ensure_polling(std::int64_t duration_us) {
    const std::int64_t until = now_us() + duration_us;
    std::int64_t previous = polling_until_us_.load();
    while (until > previous &&
           !polling_until_us_.compare_exchange_weak(previous, until)) {
    }

    std::lock_guard lock(poll_mutex_);
    if (poll_running_.load()) {
        return;
    }
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }

    poll_stop_ = false;
    poll_running_ = true;
    poll_thread_ = std::thread([this] { poll_loop(); });
}

void PointerSource::stop_polling() {
    std::lock_guard lock(poll_mutex_);
    poll_stop_ = true;
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }
    poll_running_ = false;
}

void PointerSource::poll_loop() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    HANDLE timer = CreateWaitableTimerExW(
        nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

    while (!poll_stop_.load() && !stopping_.load()) {
        if (now_us() > polling_until_us_.load()) {
            break;
        }

        sample_once(true);

        if (timer != nullptr) {
            LARGE_INTEGER due{};
            due.QuadPart = -kPollIntervalUs * 10;  // relative, 100ns units
            SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(timer, 50);
        } else {
            Sleep(8);
        }
    }

    if (timer != nullptr) {
        CloseHandle(timer);
    }
    poll_running_ = false;
}

}  // namespace cb
