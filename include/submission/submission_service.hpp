#pragma once 

#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "common/execution_config.hpp"
#include "common/execution_result.hpp"
#include "submission/submission_result.hpp"
#include "testcase/testsuite.hpp"

struct SubmissionRequest {
    std::string solution_path;
    TestSuite testsuite;
    ExecutionConfig config;
};

struct SubmissionJob {
    std::uint64_t submission_id = 0;
    SubmissionRequest request{};
    ExecutionState state = ExecutionState::Created;
    SubmissionResult result{};
};

class SubmissionQueue {
public:
    explicit SubmissionQueue(std::size_t max_pending = 0) : max_pending_(max_pending) {}

    SubmissionJob enqueue(const SubmissionRequest& request) {
        std::lock_guard<std::mutex> lock(mutex_);

        SubmissionJob job{};
        job.submission_id = std::hash<std::string>{}(request.solution_path) ^
            static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        job.request = request;
        job.state = ExecutionState::Created;
        queue_.push_back(job);
        return job;
    }

    std::optional<SubmissionJob> tryEnqueue(const SubmissionRequest& request) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (max_pending_ != 0 && queue_.size() >= max_pending_) {
            return std::nullopt;
        }

        SubmissionJob job{};
        job.submission_id = std::hash<std::string>{}(request.solution_path) ^
            static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        job.request = request;
        job.state = ExecutionState::Created;
        queue_.push_back(job);
        return job;
    }

    bool tryDequeue(SubmissionJob& job) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }

        job = queue_.front();
        queue_.pop_front();
        return true;
    }

    void complete(const SubmissionJob& job) {
        std::lock_guard<std::mutex> lock(mutex_);
        completed_.push_back(job);
    }

    bool cancel(std::uint64_t submission_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto iterator = queue_.begin(); iterator != queue_.end(); ++iterator) {
            if (iterator->submission_id != submission_id) {
                continue;
            }

            SubmissionJob cancelled = *iterator;
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

    std::vector<SubmissionJob> drainCompleted() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<SubmissionJob> drained = std::move(completed_);
        completed_.clear();
        return drained;
    }

private:
    mutable std::mutex mutex_;
    std::deque<SubmissionJob> queue_;
    std::vector<SubmissionJob> completed_;
    std::size_t max_pending_;
};

class SubmissionService{

    public:
       SubmissionResult evaluate(const std::string& solution_path,const TestSuite& testsuite,const ExecutionConfig& config);
};

class SubmissionWorker {
public:
    explicit SubmissionWorker(SubmissionQueue& queue) : queue_(&queue) {}

    bool processNext() {
        if (queue_ == nullptr) {
            return false;
        }

        SubmissionJob job{};
        if (!queue_->tryDequeue(job)) {
            return false;
        }

        job.state = ExecutionState::Running;
        SubmissionService service;
        job.result = service.evaluate(
            job.request.solution_path,
            job.request.testsuite,
            job.request.config
        );
        job.state = ExecutionState::Finished;

        queue_->complete(job);
        return true;
    }

private:
    SubmissionQueue* queue_;
};

class SubmissionWorkerPool {
public:
    explicit SubmissionWorkerPool(SubmissionQueue& queue, std::size_t worker_count = 2)
        : queue_(&queue) {
        workers_.reserve(worker_count);
        for (std::size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back(*queue_);
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
    SubmissionQueue* queue_;
    std::vector<SubmissionWorker> workers_;
};