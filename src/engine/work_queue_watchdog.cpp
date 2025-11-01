#include "work_queue_watchdog.hpp"

#include <exception>
#include <utility>

namespace sirio::engine {

namespace {
constexpr std::chrono::milliseconds kMonitorInterval{250};
constexpr std::chrono::milliseconds kStallThreshold{3000};

std::uint64_t steady_clock_now_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}
}  // namespace

WorkQueueRegistration::WorkQueueRegistration(std::function<void()> restart_callback)
    : restart_callback_(std::move(restart_callback)),
      last_heartbeat_ns_(steady_clock_now_ns()),
      restart_in_progress_(false),
      active_(true) {}

void WorkQueueRegistration::pulse() {
    last_heartbeat_ns_.store(steady_clock_now_ns(), std::memory_order_relaxed);
    active_.store(true, std::memory_order_relaxed);
}

void WorkQueueRegistration::mark_inactive() {
    active_.store(false, std::memory_order_relaxed);
}

bool WorkQueueRegistration::should_request_restart(std::uint64_t now_ns,
                                                   std::uint64_t stall_ns) {
    if (!active_.load(std::memory_order_relaxed)) {
        return false;
    }
    auto last = last_heartbeat_ns_.load(std::memory_order_relaxed);
    if (now_ns <= last || now_ns - last <= stall_ns) {
        return false;
    }
    bool expected = false;
    if (!restart_in_progress_.compare_exchange_strong(expected, true,
                                                       std::memory_order_acq_rel)) {
        return false;
    }
    return true;
}

void WorkQueueRegistration::invoke_restart() {
    try {
        if (restart_callback_) {
            restart_callback_();
        }
    } catch (const std::exception &) {
    } catch (...) {
    }
    active_.store(false, std::memory_order_relaxed);
}

WorkQueueWatchdog &WorkQueueWatchdog::instance() {
    static WorkQueueWatchdog instance;
    return instance;
}

WorkQueueWatchdog::WorkQueueWatchdog()
    : running_(true), queue_size_(0), monitor_thread_(&WorkQueueWatchdog::monitor_loop, this) {}

WorkQueueWatchdog::~WorkQueueWatchdog() { shutdown(); }

std::shared_ptr<WorkQueueRegistration> WorkQueueWatchdog::register_worker(
    std::function<void()> restart_callback) {
    auto registration = std::make_shared<WorkQueueRegistration>(std::move(restart_callback));
    registration->pulse();
    {
        std::lock_guard<std::mutex> lock(registrations_mutex_);
        registrations_.push_back(registration);
    }
    return registration;
}

void WorkQueueWatchdog::update_queue_size(std::size_t size) {
    queue_size_.store(size, std::memory_order_relaxed);
}

void WorkQueueWatchdog::shutdown() {
    bool expected = true;
    if (running_.compare_exchange_strong(expected, false)) {
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
    }
}

void WorkQueueWatchdog::monitor_loop() {
    while (running_.load(std::memory_order_relaxed)) {
        evaluate_registrations();
        std::this_thread::sleep_for(kMonitorInterval);
    }
    evaluate_registrations();
}

void WorkQueueWatchdog::evaluate_registrations() {
    std::vector<std::shared_ptr<WorkQueueRegistration>> active_registrations;
    {
        std::lock_guard<std::mutex> lock(registrations_mutex_);
        auto it = registrations_.begin();
        while (it != registrations_.end()) {
            if (auto reg = it->lock()) {
                active_registrations.push_back(std::move(reg));
                ++it;
            } else {
                it = registrations_.erase(it);
            }
        }
    }

    if (active_registrations.empty()) {
        return;
    }

    if (queue_size_.load(std::memory_order_relaxed) == 0) {
        return;
    }

    const std::uint64_t now_ns = steady_clock_now_ns();
    const std::uint64_t stall_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(kStallThreshold).count());

    for (auto &registration : active_registrations) {
        if (registration->should_request_restart(now_ns, stall_ns)) {
            registration->invoke_restart();
        }
    }
}

}  // namespace sirio::engine

