#include <gtest/gtest.h>

#include "support/fixture.hpp"
#include "common/execution_config.hpp"
#include "runner/execution_scheduler.hpp"
#include "submission/submission_service.hpp"

namespace {
const ExecutionLimits test_limits{
    .cpu_limit = std::chrono::seconds(1),
    .wall_limit = std::chrono::seconds(1),
    .memory_limit = std::size_t(1ULL * 1024 * 1024 * 1024)
};

const ExecutionConfig config{ .limit = test_limits };
}

TEST(ExecutionScheduler, AcceptsAndRunsQueuedExecution) {
    ExecutionScheduler scheduler;

    const ExecutionRequest request{
        .executable_path = getFixturePath("exit_zero"),
        .args = {},
        .input = "",
        .config = config
    };

    const auto handle = scheduler.submit(request);

    EXPECT_EQ(scheduler.pendingCount(), 1u);
    EXPECT_TRUE(scheduler.runNext());
    EXPECT_EQ(scheduler.pendingCount(), 0u);

    const auto completed = scheduler.drainCompleted();
    ASSERT_EQ(completed.size(), 1u);
    EXPECT_EQ(completed[0].execution_id, handle.execution_id);
    EXPECT_EQ(completed[0].state, ExecutionState::Finished);
    EXPECT_EQ(completed[0].result.status, ExecutionStatus::Completed);
}

TEST(ExecutionScheduler, WorkerConsumesMultipleQueuedExecutions) {
    ExecutionScheduler scheduler;
    ExecutionWorker worker(scheduler);

    const ExecutionRequest first{
        .executable_path = getFixturePath("exit_zero"),
        .args = {},
        .input = "",
        .config = config
    };

    const ExecutionRequest second{
        .executable_path = getFixturePath("exit_zero"),
        .args = {},
        .input = "",
        .config = config
    };

    const auto first_handle = scheduler.submit(first);
    const auto second_handle = scheduler.submit(second);

    EXPECT_TRUE(worker.processNext());
    EXPECT_TRUE(worker.processNext());
    EXPECT_EQ(scheduler.pendingCount(), 0u);

    const auto completed = scheduler.drainCompleted();
    ASSERT_EQ(completed.size(), 2u);
    EXPECT_TRUE(
        (completed[0].execution_id == first_handle.execution_id &&
         completed[1].execution_id == second_handle.execution_id) ||
        (completed[0].execution_id == second_handle.execution_id &&
         completed[1].execution_id == first_handle.execution_id)
    );
    EXPECT_EQ(completed[0].result.status, ExecutionStatus::Completed);
    EXPECT_EQ(completed[1].result.status, ExecutionStatus::Completed);
}

TEST(ExecutionWorkerPool, DrainsBacklogAcrossMultipleBatches) {
    ExecutionScheduler scheduler;
    ExecutionWorkerPool pool(scheduler, 2);

    const ExecutionRequest request{
        .executable_path = getFixturePath("exit_zero"),
        .args = {},
        .input = "",
        .config = config
    };

    scheduler.submit(request);
    scheduler.submit(request);
    scheduler.submit(request);

    EXPECT_EQ(pool.size(), 2u);
    EXPECT_EQ(pool.drainAll(), 3u);
    EXPECT_EQ(scheduler.pendingCount(), 0u);
    EXPECT_EQ(scheduler.drainCompleted().size(), 3u);
}

TEST(ExecutionQueue, EnqueuesAndTracksCompletion) {
    ExecutionQueue queue;

    const ExecutionRequest request{
        .executable_path = getFixturePath("exit_zero"),
        .args = {},
        .input = "",
        .config = config
    };

    const auto handle = queue.enqueue(request);
    EXPECT_EQ(queue.pendingCount(), 1u);

    ExecutionHandle dequeued{};
    EXPECT_TRUE(queue.tryDequeue(dequeued));
    EXPECT_EQ(dequeued.execution_id, handle.execution_id);
    EXPECT_EQ(queue.pendingCount(), 0u);

    dequeued.state = ExecutionState::Finished;
    dequeued.result.status = ExecutionStatus::Completed;
    queue.complete(dequeued);

    const auto completed = queue.drainCompleted();
    ASSERT_EQ(completed.size(), 1u);
    EXPECT_EQ(completed[0].state, ExecutionState::Finished);
    EXPECT_EQ(completed[0].result.status, ExecutionStatus::Completed);
}

TEST(ExecutionScheduler, CancelsQueuedExecutionWithoutRunningIt) {
    ExecutionScheduler scheduler;

    const ExecutionRequest request{
        .executable_path = getFixturePath("exit_zero"),
        .args = {},
        .input = "",
        .config = config
    };

    const auto handle = scheduler.submit(request);

    EXPECT_TRUE(scheduler.cancel(handle.execution_id));
    EXPECT_FALSE(scheduler.cancel(handle.execution_id));
    EXPECT_EQ(scheduler.pendingCount(), 0u);

    const auto completed = scheduler.drainCompleted();
    ASSERT_EQ(completed.size(), 1u);
    EXPECT_EQ(completed[0].execution_id, handle.execution_id);
    EXPECT_EQ(completed[0].state, ExecutionState::Cancelled);
    EXPECT_TRUE(scheduler.isFinished(completed[0]));
}

TEST(ExecutionQueue, RejectsJobsWhenPendingCapacityIsReached) {
    ExecutionQueue queue(1);

    const ExecutionRequest request{
        .executable_path = getFixturePath("exit_zero"),
        .args = {},
        .input = "",
        .config = config
    };

    ASSERT_TRUE(queue.tryEnqueue(request).has_value());
    EXPECT_FALSE(queue.tryEnqueue(request).has_value());
    EXPECT_EQ(queue.pendingCount(), 1u);
}

TEST(SubmissionQueue, AcceptsSubmissionRequestAndTracksJobState) {
    SubmissionQueue queue;
    TestSuite suite;
    suite.addTestCase({"", ""});

    const SubmissionRequest request{
        .solution_path = getFixturePath("exit_zero"),
        .testsuite = suite,
        .config = config
    };

    const auto job = queue.enqueue(request);
    EXPECT_EQ(queue.pendingCount(), 1u);

    SubmissionJob dequeued{};
    EXPECT_TRUE(queue.tryDequeue(dequeued));
    EXPECT_EQ(dequeued.submission_id, job.submission_id);
    EXPECT_EQ(queue.pendingCount(), 0u);

    dequeued.state = ExecutionState::Finished;
    dequeued.result = SubmissionResult{};
    queue.complete(dequeued);

    const auto completed = queue.drainCompleted();
    ASSERT_EQ(completed.size(), 1u);
    EXPECT_EQ(completed[0].state, ExecutionState::Finished);
}

TEST(SubmissionQueue, RejectsJobsWhenPendingCapacityIsReached) {
    SubmissionQueue queue(1);
    TestSuite suite;
    suite.addTestCase({"", ""});

    const SubmissionRequest request{
        .solution_path = getFixturePath("exit_zero"),
        .testsuite = suite,
        .config = config
    };

    ASSERT_TRUE(queue.tryEnqueue(request).has_value());
    EXPECT_FALSE(queue.tryEnqueue(request).has_value());
    EXPECT_EQ(queue.pendingCount(), 1u);
}

TEST(SubmissionQueue, CancelsQueuedJobAndPublishesCancelledState) {
    SubmissionQueue queue;
    TestSuite suite;
    suite.addTestCase({"", ""});

    const SubmissionRequest request{
        .solution_path = getFixturePath("exit_zero"),
        .testsuite = suite,
        .config = config
    };

    const auto job = queue.enqueue(request);

    EXPECT_TRUE(queue.cancel(job.submission_id));
    EXPECT_FALSE(queue.cancel(job.submission_id));
    EXPECT_EQ(queue.pendingCount(), 0u);

    const auto completed = queue.drainCompleted();
    ASSERT_EQ(completed.size(), 1u);
    EXPECT_EQ(completed[0].submission_id, job.submission_id);
    EXPECT_EQ(completed[0].state, ExecutionState::Cancelled);
}

TEST(SubmissionWorker, ProcessesQueuedSubmissionJobs) {
    SubmissionQueue queue;
    SubmissionWorker worker(queue);
    TestSuite suite;
    suite.addTestCase({"", ""});

    const SubmissionRequest request{
        .solution_path = getFixturePath("exit_zero"),
        .testsuite = suite,
        .config = config
    };

    const auto job = queue.enqueue(request);

    EXPECT_TRUE(worker.processNext());
    EXPECT_EQ(queue.pendingCount(), 0u);

    const auto completed = queue.drainCompleted();
    ASSERT_EQ(completed.size(), 1u);
    EXPECT_EQ(completed[0].submission_id, job.submission_id);
    EXPECT_EQ(completed[0].state, ExecutionState::Finished);
}

TEST(SubmissionWorkerPool, DrainsMultipleQueuedSubmissionJobs) {
    SubmissionQueue queue;
    SubmissionWorkerPool pool(queue, 2);
    TestSuite suite;
    suite.addTestCase({"", ""});

    const SubmissionRequest first{
        .solution_path = getFixturePath("exit_zero"),
        .testsuite = suite,
        .config = config
    };

    const SubmissionRequest second{
        .solution_path = getFixturePath("exit_zero"),
        .testsuite = suite,
        .config = config
    };

    const auto first_job = queue.enqueue(first);
    const auto second_job = queue.enqueue(second);

    EXPECT_EQ(pool.drainOnce(), 2u);
    EXPECT_EQ(queue.pendingCount(), 0u);

    const auto completed = queue.drainCompleted();
    ASSERT_EQ(completed.size(), 2u);
    EXPECT_TRUE(
        (completed[0].submission_id == first_job.submission_id &&
         completed[1].submission_id == second_job.submission_id) ||
        (completed[0].submission_id == second_job.submission_id &&
         completed[1].submission_id == first_job.submission_id)
    );
    EXPECT_EQ(completed[0].state, ExecutionState::Finished);
    EXPECT_EQ(completed[1].state, ExecutionState::Finished);
}

TEST(SubmissionWorkerPool, DrainsBacklogAcrossMultipleBatches) {
    SubmissionQueue queue;
    SubmissionWorkerPool pool(queue, 2);
    TestSuite suite;
    suite.addTestCase({"", ""});

    const SubmissionRequest request{
        .solution_path = getFixturePath("exit_zero"),
        .testsuite = suite,
        .config = config
    };

    queue.enqueue(request);
    queue.enqueue(request);
    queue.enqueue(request);

    EXPECT_EQ(pool.drainAll(), 3u);
    EXPECT_EQ(queue.pendingCount(), 0u);
    EXPECT_EQ(queue.drainCompleted().size(), 3u);
}
