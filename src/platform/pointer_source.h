#pragma once

#include "core/types.h"

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

namespace cb {

class MonitorService;

class SampleQueue {
public:
    SampleQueue();
    ~SampleQueue();

    SampleQueue(const SampleQueue&) = delete;
    SampleQueue& operator=(const SampleQueue&) = delete;

    void push(const Sample& sample);
    bool pop(Sample& sample);
    bool wait(int timeout_ms);
    [[nodiscard]] std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::array<Sample, 512> buffer_{};
    std::size_t head_{};
    std::size_t count_{};
    HANDLE event_{};
};

class PointerSource {
public:
    enum class Mode {
        RawInput,
        Polling,
    };

    PointerSource(SampleQueue& queue, MonitorService& monitors);
    ~PointerSource();

    PointerSource(const PointerSource&) = delete;
    PointerSource& operator=(const PointerSource&) = delete;

    bool start();
    void stop();

    [[nodiscard]] bool running() const { return running_.load(); }
    [[nodiscard]] Mode mode() const;
    [[nodiscard]] bool raw_input_available() const { return raw_available_.load(); }
    [[nodiscard]] std::uint64_t raw_events() const { return raw_events_.load(); }
    [[nodiscard]] std::uint64_t polled_samples() const { return polled_samples_.load(); }
    [[nodiscard]] std::uint64_t watchdog_checks() const { return watchdog_checks_.load(); }

private:
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handle_message(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    bool initialize();
    void message_loop();
    void handle_raw_input(LPARAM lparam);
    void handle_watchdog();
    void sample_once(bool polled);

    void ensure_polling(std::int64_t duration_us);
    void stop_polling();
    void poll_loop();

    SampleQueue& queue_;
    MonitorService& monitors_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
    std::atomic<HWND> hwnd_{nullptr};

    std::atomic<bool> raw_available_{false};
    std::atomic<std::uint64_t> raw_events_{0};
    std::atomic<std::uint64_t> polled_samples_{0};
    std::atomic<std::uint64_t> watchdog_checks_{0};

    std::atomic<std::int64_t> last_raw_event_us_{0};
    std::atomic<std::int64_t> last_sample_us_{0};
    std::atomic<std::int32_t> last_x_{0};
    std::atomic<std::int32_t> last_y_{0};

    std::atomic<bool> poll_running_{false};
    std::atomic<bool> poll_stop_{false};
    std::atomic<std::int64_t> polling_until_us_{0};
    std::thread poll_thread_;
    std::mutex poll_mutex_;

    static constexpr UINT_PTR kWatchdogTimer = 1;
    static constexpr UINT kStopMessage = WM_APP + 1;
};

}  // namespace cb
