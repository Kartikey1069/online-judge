#pragma once

#include <deque>
#include <mutex>
#include <optional>
#include <vector>

#include "common/execution_config.hpp"
#include "common/execution_result.hpp"
#include "runner/process_runner.hpp"

struct ExecutionRequest {
    std::string executable_path;
    std::vector<std::string> args;
    std::string input;
    ExecutionConfig config;
};

struct ExecutionHandle {
    std::uint64_t execution_id = 0;
    ExecutionRequest request{};
    ExecutionState state = ExecutionState::Created;
    ExecutionResult result{};
};

class ExecutionQueue {
public:
    explicit ExecutionQueue(std::size_t max_pending = 0) : max_pending_(max_pending) {}

    ExecutionHandle enqueue(const ExecutionRequest& request) {
        std::lock_guard<std::mutex> lock(mutex_);

        ExecutionHandle handle{};
        handle.execution_id = ProcessRunner::generateExecutionId();
        handle.request = request;
        handle.state = ExecutionState::Created;
        queue_.push_back(handle);
        return handle;
    }

    std::optional<ExecutionHandle> tryEnqueue(const ExecutionRequest& request) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (max_pending_ != 0 && queue_.size() >= max_pending_) {
            return std::nullopt;
        }

        ExecutionHandle handle{};
        handle.execution_id = ProcessRunner::generateExecutionId();
        handle.request = request;
        handle.state = ExecutionState::Created;
        queue_.push_back(handle);
        return handle;
    }

    bool tryDequeue(ExecutionHandle& handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }

        handle = queue_.front();
        queue_.pop_front();
        return true;
    }

    void complete(const ExecutionHandle& handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        completed_.push_back(handle);
    }

    bool cancel(std::uint64_t execution_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto iterator = queue_.begin(); iterator != queue_.end(); ++iterator) {
            if (iterator->execution_id != execution_id) {
                continue;
            }

            ExecutionHandle cancelled = *iterator;
            cancelled.state = ExecutionState::Cancelled;
            queue_.erase(iterator);
            completed_.push_back(cancelled);
            return true;
        }
        return false;
    }

    std::size_t pendingCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    std::vector<ExecutionHandle> drainCompleted() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ExecutionHandle> drained = std::move(completed_);
        completed_.clear();
        return drained;
    }

private:
    mutable std::mutex mutex_;
    std::deque<ExecutionHandle> queue_;
    std::vector<ExecutionHandle> completed_;
    std::size_t max_pending_;
};

class ExecutionScheduler {
public:
    ExecutionHandle submit(const ExecutionRequest& request) {
        return queue_.enqueue(request);
    }

    bool runNext() {
        ExecutionHandle handle{};
        if (!queue_.tryDequeue(handle)) {
            return false;
        }

        handle.state = ExecutionState::Running;
        handle.result = runner_.run(
            handle.request.executable_path,
            handle.request.args,
            handle.request.input,
            handle.request.config
        );
        handle.state = handle.result.state;

        queue_.complete(handle);
        return true;
    }

    std::size_t pendingCount() const {
        return queue_.pendingCount();
    }

    bool cancel(std::uint64_t execution_id) {
        return queue_.cancel(execution_id);
    }

    std::vector<ExecutionHandle> drainCompleted() {
        return queue_.drainCompleted();
    }

    bool isFinished(const ExecutionHandle& handle) const {
        return handle.state == ExecutionState::Finished ||
               handle.state == ExecutionState::TimedOut ||
               handle.state == ExecutionState::Signaled ||
               handle.state == ExecutionState::Failed ||
               handle.state == ExecutionState::SandboxFailed ||
               handle.state == ExecutionState::RunnerFailed ||
               handle.state == ExecutionState::Cancelled;
    }

private:
    ProcessRunner runner_;
    ExecutionQueue queue_;
};

class ExecutionWorker {
public:
    explicit ExecutionWorker(ExecutionScheduler& scheduler) : scheduler_(&scheduler) {}

    bool processNext() {
        if (scheduler_ == nullptr) {
            return false;
        }

        return scheduler_->runNext();
    }

private:
    ExecutionScheduler* scheduler_;
};

class ExecutionWorkerPool {
public:
    explicit ExecutionWorkerPool(ExecutionScheduler& scheduler, std::size_t worker_count = 2)
        : scheduler_(&scheduler) {
        workers_.reserve(worker_count);
        for (std::size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back(*scheduler_);
        }
    }

    std::size_t size() const {
        return workers_.size();
    }

    std::size_t drainOnce() {
        std::size_t processed = 0;
        for (auto& worker : workers_) {
            if (worker.processNext()) {
                ++processed;
            }
        }
        return processed;
    }

    std::size_t drainAll() {
        std::size_t processed = 0;
        std::size_t batch = drainOnce();
        while (batch != 0) {
            processed += batch;
            batch = drainOnce();
        }
        return processed;
    }

private:
    ExecutionScheduler* scheduler_;
    std::vector<ExecutionWorker> workers_;
};
