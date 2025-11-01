#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace sirio::engine {

class WorkQueueRegistration : public std::enable_shared_from_this<WorkQueueRegistration> {
public:
    explicit WorkQueueRegistration(std::function<void()> restart_callback);

    WorkQueueRegistration(const WorkQueueRegistration &) = delete;
    WorkQueueRegistration &operator=(const WorkQueueRegistration &) = delete;

    void pulse();
    void mark_inactive();

private:
    friend class WorkQueueWatchdog;

    bool should_request_restart(std::uint64_t now_ns, std::uint64_t stall_ns);
    void invoke_restart();

    std::function<void()> restart_callback_;
    std::atomic<std::uint64_t> last_heartbeat_ns_;
    std::atomic<bool> restart_in_progress_;
    std::atomic<bool> active_;
};

class WorkQueueWatchdog {
public:
    static WorkQueueWatchdog &instance();

    std::shared_ptr<WorkQueueRegistration> register_worker(std::function<void()> restart_callback);
    void update_queue_size(std::size_t size);
    void shutdown();

private:
    WorkQueueWatchdog();
    ~WorkQueueWatchdog();

    WorkQueueWatchdog(const WorkQueueWatchdog &) = delete;
    WorkQueueWatchdog &operator=(const WorkQueueWatchdog &) = delete;

    void monitor_loop();
    void evaluate_registrations();

    std::atomic<bool> running_;
    std::atomic<std::size_t> queue_size_;
    std::mutex registrations_mutex_;
    std::vector<std::weak_ptr<WorkQueueRegistration>> registrations_;
    std::thread monitor_thread_;
};

}  // namespace sirio::engine

