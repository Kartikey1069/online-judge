Absolutely. This is a good point to create a **canonical project roadmap/context document**.

Your previous README is good as a public-facing README, but it is **not detailed enough to preserve our engineering context**. It doesn't capture:

* why we made architectural decisions
* what has actually been implemented
* what has been tested
* why interfaces/mocks exist
* what M7 is supposed to teach
* the intended progression into OS/security/backend/distributed systems
* technical debt
* deferred work
* testing philosophy
* design constraints
* interview/resume goals
* what should *not* be implemented prematurely

I would use the following as the new **master project roadmap**. It can serve both as the README and as the context document you give me in future project conversations.

---

# Online Judge — Engineering Roadmap & Project Context

````markdown
# Online Judge

A production-oriented Online Judge built from scratch in Modern C++ on Linux.

This project is intentionally being developed as a systems-engineering learning project rather than merely as a coding-platform clone.

The goal is to understand, design, implement, test, debug, secure, benchmark, and eventually scale the same classes of systems used by real online judges and production backend platforms.

The project is being built from first principles with particular emphasis on:

- Linux systems programming
- Operating systems
- Process management
- Inter-process communication
- C++ architecture
- Resource management
- Security
- Sandboxing
- Backend engineering
- Databases
- Concurrency
- Distributed systems
- Observability
- Performance engineering
- Production system design

The final system should be strong enough to serve as a serious systems/backend engineering project for a software-engineering portfolio and technical interviews.

---

# 1. Project Philosophy

This project is not optimized for reaching a working implementation as quickly as possible.

The primary objective is engineering understanding.

Every major subsystem should be approached through the following progression:

1. Understand the real-world problem.
2. Identify the requirements.
3. Identify constraints and failure modes.
4. Reason about possible designs.
5. Compare alternatives and trade-offs.
6. Understand the relevant OS/network/database/concurrency concepts.
7. Design the architecture.
8. Implement incrementally.
9. Write focused tests.
10. Intentionally break the implementation where useful.
11. Verify that tests detect the failure.
12. Review correctness and production-readiness.
13. Optimize where justified by measurements.
14. Document the design.
15. Prepare interview and resume explanations.

The project should prioritize:

- Understanding over memorization
- Design reasoning over API memorization
- Correctness over premature optimization
- Explicit failure handling
- Separation of concerns
- Testability
- Security
- Measurable performance
- Maintainability
- Production thinking

---

# 2. Current Engineering Objective

The immediate objective is to transform the current basic Online Judge into a progressively more realistic execution platform.

The project will eventually evolve from:

```text
Source Code
    |
    v
Compiler
    |
    v
ProcessRunner
    |
    v
Judge
    |
    v
SubmissionResult
````

into a production-oriented architecture capable of:

* executing untrusted programs
* applying CPU and memory limits
* handling process failures
* handling signals
* safely capturing large outputs
* isolating processes
* running submissions concurrently
* persisting results
* exposing a backend API
* processing submissions through workers
* scaling horizontally
* providing observability
* running inside containers/sandboxes

---

# 3. Current Architecture

The current conceptual pipeline is:

```text
                         SubmissionService
                                |
                                v
                            Compiler
                                |
                                v
                         ProcessRunner
                                |
                                v
                         ExecutionResult
                                |
                                v
                              Judge
                                |
                                v
                           JudgeResult
                                |
                                v
                        SubmissionResult
```

The system also contains a dedicated `TestRunner` responsible for executing a test suite:

```text
                         TestRunner
                        /          \
                       /            \
                      v              v
              IProcessRunner       IJudge
                    ^                  ^
                    |                  |
             ProcessRunner           Judge
```

The abstraction boundary allows TestRunner to be unit-tested independently of the real process-execution subsystem and real judging implementation.

---

# 4. Core Components

## 4.1 ProcessRunner

### Responsibility

`ProcessRunner` is the low-level Linux process execution infrastructure.

It is responsible for:

* creating child processes
* replacing the child process image
* passing command-line arguments
* redirecting stdin
* capturing stdout
* capturing stderr
* monitoring child processes
* waiting for process termination
* collecting execution information
* handling process-level failures

It should NOT decide:

* whether a submission is Accepted
* whether output is correct
* whether a submission should receive Wrong Answer
* whether a test case is logically correct

Those decisions belong to higher layers.

### Current implementation

The implementation currently uses Linux/POSIX mechanisms including:

* `fork()`
* `execvp()`
* `pipe()`
* `dup2()`
* `waitpid()`
* file descriptors
* nonblocking I/O
* `poll()`

### Conceptual execution flow

```text
Parent
  |
  +-- create stdin pipe
  |
  +-- create stdout pipe
  |
  +-- create stderr pipe
  |
  +-- fork()
       |
       +----------------------+
       |                      |
      Child                  Parent
       |                      |
       +-- dup2(stdin)        +-- write input
       +-- dup2(stdout)       +-- poll/read stdout
       +-- dup2(stderr)       +-- poll/read stderr
       +-- execvp()           +-- poll/read stderr
                              +-- monitor process
                              +-- waitpid()
                              +-- construct ExecutionResult
```

---

# 5. ExecutionResult

`ExecutionResult` represents facts produced by process execution.

It should contain information such as:

```text
ExecutionResult
├── exit_code
├── stdout_output
├── stderr_output
├── execution_time
├── memory_used
└── termination information
```

The important architectural principle is:

> ProcessRunner reports execution facts. It does not interpret them as a final submission verdict.

For example:

```text
exit_code = 0
stdout = "42\n"
```

does not itself mean Accepted.

The Judge decides that.

---

# 6. TestSuite and TestCase

A `TestCase` currently represents:

```text
input
expected_output
```

A `TestSuite` contains multiple test cases and provides:

* adding test cases
* retrieving a test case
* determining suite size

Conceptually:

```text
TestSuite
├── TestCase 0
│   ├── input
│   └── expected_output
├── TestCase 1
│   ├── input
│   └── expected_output
└── ...
```

---

# 7. TestRunner

`TestRunner` is responsible for executing a submission against a `TestSuite`.

Its responsibilities are:

1. Iterate through test cases.
2. Pass testcase input to the process runner.
3. Pass the resulting `ExecutionResult` to the Judge.
4. Stop at the first non-Accepted verdict.
5. Return information about the failed test.
6. Return an empty failure result if all tests pass.

Current conceptual algorithm:

```text
for each testcase:

    execution_result =
        ProcessRunner.run(executable, args, testcase.input)

    judge_result =
        Judge.evaluate(
            execution_result,
            testcase.expected_output
        )

    if judge_result != Accepted:
        return failure information

return success
```

The system intentionally uses a fail-fast model.

---

# 8. Dependency Inversion

TestRunner does not directly depend on concrete implementations.

Instead:

```text
IProcessRunner
       ^
       |
ProcessRunner

IJudge
  ^
  |
Judge
```

TestRunner depends on:

```text
IProcessRunner
IJudge
```

This allows:

```text
Production:

TestRunner
    |
    +-- ProcessRunner
    +-- Judge
```

and:

```text
Unit Tests:

TestRunner
    |
    +-- MockProcessRunner
    +-- MockJudge
```

This is dependency injection and runtime polymorphism.

The purpose is not abstraction for its own sake.

The purpose is:

* isolation
* testability
* lower coupling
* controlled failure simulation
* clearer architectural boundaries

---

# 9. Testing Architecture

Testing is treated as an engineering subsystem rather than simply a collection of assertions.

## ProcessRunner tests

ProcessRunner is tested using real Linux fixture programs.

Fixtures include programs for:

* normal exit
* nonzero exit
* stdin echo
* stdout output
* stderr output
* stdout + stderr
* no trailing newline
* command-line arguments
* binary output
* large output
* large input/output
* signal termination

This ensures that ProcessRunner tests exercise actual:

* pipes
* file descriptors
* fork
* exec
* polling
* process lifecycle
* stream handling

rather than mocks.

---

# 10. TestRunner Tests

TestRunner is tested using mocks because it should not need to launch real Linux processes.

Mock infrastructure:

```text
MockProcessRunner
MockJudge
```

Tests currently cover:

* all test cases accepted
* stopping on first failure
* preserving failed test details
* not executing tests after failure
* forwarding testcase input correctly
* forwarding expected output correctly
* empty test suite
* preserving non-Accepted verdicts

GoogleMock is used for:

* `EXPECT_CALL`
* `WillOnce`
* `Return`
* `Times`
* `_`
* `Field`
* `AllOf`

The important testing principle is:

> A test should verify the responsibility of the component under test without unnecessarily re-testing lower-level components.

---

# 11. Current Testing Status

```text
ProcessRunner
    [x] Unit/integration-style Linux execution tests
    [x] stdin
    [x] stdout
    [x] stderr
    [x] arguments
    [x] exit status
    [x] signal-related fixtures
    [x] large input/output fixtures

TestSuite
    [x] Tested

Judge
    [x] Tested

TestRunner
    [x] Tested with mocks

Compiler
    [ ] Testing intentionally deferred until after M7

SubmissionService
    [ ] Testing intentionally deferred until after M7
```

Compiler and SubmissionService testing is intentionally postponed because M7 will change the execution architecture and resource-management model.

Tests should be aligned with the final architecture rather than written prematurely against a transitional design.

---

# 12. Compiler

Compiler currently wraps process execution to invoke the compiler toolchain.

Current behavior:

```text
source.cpp
    |
    v
g++
    |
    +-- compilation failure
    |
    +-- compilation success
            |
            v
       executable
```

Current compiler command is conceptually:

```text
g++ source.cpp -o executable
```

`CompileResult` contains:

* compiler exit code
* compiler stderr
* executable path on success

Current architecture directly creates a `ProcessRunner`.

This will eventually be refactored to dependency injection:

```text
Compiler
    |
    v
IProcessRunner
```

allowing:

```text
Compiler
   |
   +-- ProcessRunner       production
   |
   +-- MockProcessRunner   tests
```

Compiler tests are intentionally deferred until this refactor and M7 execution changes are complete.

---

# 13. SubmissionService

SubmissionService is the high-level orchestration layer.

Conceptual responsibility:

```text
Source Code
    |
    v
Compiler
    |
    +---- failure ----> Compilation Error
    |
    v
Executable
    |
    v
TestRunner / execution pipeline
    |
    v
Judge
    |
    v
SubmissionResult
```

It should coordinate components rather than implement their internal logic.

Important principle:

> SubmissionService should orchestrate; it should not become a second ProcessRunner, Compiler, or Judge.

Testing will be added after M7 once the execution model stabilizes.

---

# 14. Result Model

The system uses explicit result structures.

## CompileResult

Contains:

```text
exit_code
stderr_output
executable_path
```

## ExecutionResult

Contains execution facts:

```text
exit_code
stdout_output
stderr_output
execution_time
memory_used
termination information
```

## JudgeResult

Contains:

```text
verdict
```

and may eventually contain additional judging metadata.

## TestRunnerResult

Represents either:

```text
all tests passed
```

or:

```text
failed test index
failed execution result
failed judge result
```

`std::optional` is used to explicitly represent information that does not exist.

Example:

```text
All tests accepted:

failed_test_index       = nullopt
failed_execution_result = nullopt
failed_judge_result     = nullopt
```

Example:

```text
Test 3 failed:

failed_test_index       = 3
failed_execution_result = present
failed_judge_result     = present
```

---

# 15. Current M7 — Execution Platform

M7 transforms the current basic process executor into a resource-aware execution platform.

## Completed

### Standard input redirection

[x]

The parent can provide arbitrary test input to the child's stdin using pipes and file descriptor redirection.

### Multiple test case execution

[x]

TestSuite + TestRunner allow a submission to be executed against multiple test cases.

### Robust streaming output capture

[x]

The current execution mechanism uses polling/nonblocking stream handling instead of a simplistic blocking read approach.

This is important because stdout/stderr can fill pipe buffers and cause deadlocks if handled incorrectly.

---

# 16. M7 Remaining Work

## Execution time measurement

[ ]

Measure the duration of process execution.

Important questions:

* When does measurement start?
* Before fork or after fork?
* Should process creation time be included?
* Which clock should be used?
* Why should a monotonic clock be preferred?
* What precision is needed?
* How should measurement interact with TLE?

Expected concept:

```text
start
  |
  v
process execution
  |
  v
termination
  |
  v
end

execution_time = end - start
```

---

# 17. Memory Measurement

[ ]

Determine how much memory a submission consumes.

Important questions:

* What exactly is memory usage?
* Peak RSS vs virtual memory?
* Parent memory vs child memory?
* How do we obtain child memory usage on Linux?
* What happens with multiple processes?
* How accurate must measurement be?
* Can memory be monitored continuously?
* How does measurement interact with MLE?

This should eventually support:

```text
memory_used
```

inside `ExecutionResult`.

---

# 18. Time Limit Exceeded

[ ]

TLE should become a resource-enforcement mechanism rather than merely a measurement.

Conceptually:

```text
start process
     |
     v
monitor execution
     |
     +---- completes before limit
     |          |
     |          v
     |      normal result
     |
     +---- exceeds limit
                |
                v
           terminate process
                |
                v
             TLE result
```

Important engineering questions:

* Who monitors time?
* How frequently?
* What happens if the process is blocked?
* What happens if it ignores SIGTERM?
* Should we use SIGTERM followed by SIGKILL?
* How do we prevent zombies?
* How do we classify the resulting termination?
* How does this interact with process groups?

---

# 19. Memory Limit Exceeded

[ ]

Memory enforcement should eventually behave similarly:

```text
process
   |
   v
memory usage
   |
   +---- below limit → continue
   |
   +---- above limit → terminate
```

Potential Linux mechanisms to investigate:

* `setrlimit()`
* `RLIMIT_AS`
* `RLIMIT_DATA`
* `RLIMIT_RSS`
* cgroups
* container resource limits

The implementation should be selected based on actual semantics rather than simply using whichever API is easiest.

---

# 20. Runtime Signal Handling

[ ]

The executor should distinguish:

```text
normal exit
```

from:

```text
terminated by signal
```

Potential cases:

```text
SIGSEGV → segmentation fault
SIGABRT → abort
SIGFPE  → arithmetic error
SIGKILL → killed
SIGTERM → terminated
```

The result model should preserve enough information to allow the higher layers to distinguish these cases.

This is why execution termination information belongs in `ExecutionResult`.

---

# 21. Large Output Handling

[ ]

Large output must not cause:

* deadlock
* uncontrolled memory growth
* blocking forever
* pipe buffer deadlock
* accidental truncation without reporting

Current streaming/polling infrastructure provides the foundation.

Future decisions include:

* maximum captured output
* output truncation policy
* output limit violation
* whether stdout and stderr have independent limits
* whether excess output causes immediate termination
* how the result is classified

Potential future verdict:

```text
Output Limit Exceeded
```

if we decide it belongs in the judging model.

---

# 22. Process Cleanup and Failure Recovery

[ ]

This is one of the most important production concerns.

Every execution path must account for:

```text
fork failure
exec failure
pipe failure
poll failure
read failure
write failure
child termination
timeout
memory violation
signal termination
unexpected parent failure
```

The executor must avoid:

* zombie processes
* leaked file descriptors
* orphaned children
* hung pipes
* inconsistent result states

The general invariant should be:

> Every child process created by ProcessRunner must have a well-defined cleanup/reaping path.

---

# 23. M7 Architectural Goal

At the end of M7, ProcessRunner should evolve from:

```text
"run a process and capture output"
```

into:

```text
"execute an untrusted process under controlled resource and lifecycle constraints"
```

The conceptual architecture should become:

```text
                 ProcessRunner
                      |
          +-----------+-----------+
          |           |           |
        Input       Output      Monitor
          |           |           |
          |           |     +-----+------+
          |           |     |            |
          |           |    Time        Memory
          |           |     |            |
          |           |    TLE          MLE
          |           |     |
          +-----------+-----+
                      |
                Process Control
                      |
             +--------+--------+
             |                 |
          Signals           Cleanup
```

---

# 24. Security & Sandboxing Roadmap

After M7:

```text
[ ] Process isolation
[ ] Linux resource limits
[ ] Restricted filesystem access
[ ] Restricted system calls
[ ] Linux namespaces
[ ] cgroups
[ ] seccomp
[ ] Docker sandbox
[ ] Container resource limits
[ ] Secure execution environment
```

Important security principle:

> Resource limits are not the same thing as sandboxing.

A process that is limited to 1 CPU second is not necessarily safe.

A malicious program may still:

* access the filesystem
* inspect environment variables
* communicate with the network
* spawn processes
* exploit kernel vulnerabilities
* access sensitive resources

Therefore security must eventually be treated as a layered system.

---

# 25. Sandbox Architecture

The eventual execution environment should conceptually become:

```text
Submission
    |
    v
Execution Worker
    |
    v
Sandbox
    |
    +-- PID isolation
    +-- filesystem isolation
    +-- network restrictions
    +-- CPU limits
    +-- memory limits
    +-- process limits
    +-- syscall restrictions
    +-- timeout
    +-- output limits
```

Potential technologies:

* Linux namespaces
* cgroups
* seccomp
* chroot/pivot_root where appropriate
* Docker
* container runtime isolation

The project should first understand the Linux primitives before hiding them behind Docker.

---

# 26. Backend Roadmap

After execution and security fundamentals are stable:

```text
[ ] Submission API
[ ] REST API
[ ] Authentication
[ ] Submission persistence
[ ] Database integration
[ ] Result persistence
[ ] Problem management
[ ] Submission history
```

Potential architecture:

```text
Client
   |
   v
REST API
   |
   v
Submission Service
   |
   v
Job Queue
   |
   v
Execution Workers
   |
   v
Sandbox
   |
   v
Result
   |
   v
Database
```

---

# 27. Database Roadmap

Eventually introduce:

* SQL
* schema design
* transactions
* indexes
* query planning
* connection pooling
* consistency
* persistence failures
* migrations

Potential entities:

```text
User
Problem
TestCase
Submission
SubmissionResult
Execution
```

Important design questions:

* What belongs in SQL?
* What is immutable?
* What should be normalized?
* What should be indexed?
* What data should be cached?
* What is the source of truth?
* How should concurrent submissions update state?

---

# 28. Concurrency Roadmap

Once a single submission works reliably:

```text
[ ] Submission queue
[ ] Worker pool
[ ] Concurrent execution
[ ] Bounded job queue
[ ] Worker failure handling
[ ] Scheduler
[ ] Backpressure
```

Conceptual system:

```text
                API
                 |
                 v
             Job Queue
                 |
        +--------+--------+
        |        |        |
        v        v        v
     Worker   Worker   Worker
        |        |        |
        v        v        v
     Sandbox  Sandbox  Sandbox
```

Important concepts:

* threads
* mutexes
* condition variables
* producer/consumer
* bounded queues
* backpressure
* graceful shutdown
* cancellation
* worker isolation

---

# 29. Distributed Systems Roadmap

After local concurrency:

```text
[ ] Distributed execution
[ ] Horizontal scaling
[ ] Worker registration
[ ] Scheduling
[ ] Worker health
[ ] Failure detection
[ ] Retry policies
[ ] Idempotency
[ ] Job ownership
[ ] Result delivery
```

Potential architecture:

```text
                 API Servers
                     |
                     v
                Job Queue
                     |
          +----------+----------+
          |          |          |
          v          v          v
       Worker A   Worker B   Worker C
          |          |          |
       Sandbox    Sandbox    Sandbox
```

Questions to study:

* What if a worker dies?
* What if a job is executed twice?
* What if result delivery fails?
* How do we avoid duplicate submissions?
* How do we know a worker is healthy?
* Where does job ownership live?
* How does scheduling work?
* What consistency guarantees are required?

---

# 30. Observability & Performance

Eventually introduce:

```text
[ ] Structured logging
[ ] Execution metrics
[ ] Submission latency
[ ] Throughput
[ ] P50 latency
[ ] P95 latency
[ ] P99 latency
[ ] CPU utilization
[ ] Memory utilization
[ ] Benchmark suite
[ ] Sequential vs concurrent benchmarks
```

Performance claims must be measured.

Do not write:

```text
"10x faster"
```

without a reproducible benchmark.

Benchmark methodology should document:

* workload
* machine
* compiler
* build mode
* number of iterations
* warmup
* measurement method
* statistical summary

---

# 31. Deployment Roadmap

Eventually:

```text
[ ] Dockerized services
[ ] Production configuration
[ ] CI/CD
[ ] Deployment
[ ] Monitoring
[ ] Health checks
```

The deployment architecture should eventually resemble:

```text
                 Client
                   |
                   v
              API Service
                   |
                   v
               Job Queue
                   |
          +--------+--------+
          |        |        |
          v        v        v
       Worker   Worker   Worker
          |        |        |
       Sandbox  Sandbox  Sandbox
          |        |        |
          +--------+--------+
                   |
                   v
               Database
```

---

# 32. Testing Strategy by Layer

Testing should evolve with architecture.

## ProcessRunner

Use real Linux processes and fixture executables.

Reason:

The interesting behavior is the Linux interaction itself.

Test:

* fork
* exec
* pipes
* stdin
* stdout
* stderr
* signals
* large streams
* process termination

## TestRunner

Use mocks.

Reason:

The interesting behavior is orchestration.

Test:

* dependency calls
* ordering
* fail-fast
* result propagation
* empty suites
* forwarding arguments

## Judge

Use pure unit tests.

Reason:

Judge should be deterministic business logic.

## Compiler

Eventually use dependency injection.

Then:

```text
Compiler
    |
    +-- MockProcessRunner
```

Tests should simulate:

* compiler success
* compiler failure
* compiler diagnostics
* output executable path

## SubmissionService

Use mocked dependencies.

Test:

* compilation failure stops pipeline
* successful compilation proceeds
* execution failure behavior
* judging behavior
* final SubmissionResult
* optional stage semantics

## Integration tests

Eventually test:

```text
Source
  |
Compiler
  |
ProcessRunner
  |
TestRunner
  |
Judge
  |
SubmissionResult
```

using real components.

This prevents excessive mocking from hiding integration problems.

---

# 33. Testing Philosophy

Tests should verify behavior rather than implementation details.

Good test:

```text
"TestRunner stops after the first failed testcase."
```

Bad test:

```text
"TestRunner uses this exact local variable."
```

Mocks should be:

* strict enough to detect incorrect behavior
* loose enough to avoid irrelevant coupling

Avoid adding production functionality purely to satisfy tests.

Example:

Do not add:

```cpp
operator==(ExecutionResult)
```

solely because GoogleMock wants to compare an object.

Instead use appropriate matchers such as:

* `Field`
* `AllOf`
* `Ref`
* `_`

depending on the contract being tested.

---

# 34. Current Project Structure

Current conceptual structure:

```text
include/
├── common/
│   ├── execution_result.hpp
│   └── verdict.hpp
│
├── compiler/
│   ├── compiler.hpp
│   └── compile_result.hpp
│
├── judge/
│   ├── judge.hpp
│   ├── i_judge.hpp
│   └── judge_result.hpp
│
├── runner/
│   ├── process_runner.hpp
│   └── i_process_runner.hpp
│
├── submission/
│   ├── submission_service.hpp
│   └── submission_result.hpp
│
├── testcase/
│   ├── test_case.hpp
│   └── testsuite.hpp
│
└── testrunner/
    ├── testrunner.hpp
    └── testrunner_result.hpp

src/
├── compiler/
├── judge/
├── runner/
├── submission/
├── testcase/
├── testrunner/
└── verdict/

tests/
├── fixtures/
├── mocks/
├── judge/
├── runner/
├── testcase/
├── testrunner/
├── support/
└── test_main.cpp
```

---

# 35. Important Architectural Decisions

## Decision 1 — ProcessRunner is reusable infrastructure

ProcessRunner should not know about:

* test cases
* Accepted
* Wrong Answer
* submissions
* problems

It only executes processes.

---

## Decision 2 — Judge is separate from execution

Judge receives execution facts and determines the verdict.

This keeps execution and evaluation independently testable.

---

## Decision 3 — TestRunner uses dependency injection

TestRunner depends on:

```text
IProcessRunner
IJudge
```

rather than concrete implementations.

This enables deterministic unit testing.

---

## Decision 4 — Interfaces are introduced where substitution is useful

Interfaces should not be added everywhere automatically.

The purpose is dependency inversion and testability, not abstraction for abstraction's sake.

---

## Decision 5 — Explicit result types

Information is passed through structured result objects instead of hidden global state or exceptions for normal control flow.

---

## Decision 6 — Failure stages are explicit

`std::optional` represents stages that were never reached.

This makes partial execution explicit.

---

## Decision 7 — Fail fast during test execution

Once a testcase fails:

```text
stop executing remaining tests
```

This saves execution time and simplifies failure reporting.

---

# 36. Technical Debt

Known areas requiring future work:

```text
[ ] Compiler dependency injection
[ ] SubmissionService dependency injection
[ ] Compiler tests
[ ] SubmissionService tests
[ ] Resource limits
[ ] Robust signal classification
[ ] Process groups
[ ] Child cleanup
[ ] Memory accounting
[ ] Timeout enforcement
[ ] Output limits
[ ] Sandbox isolation
[ ] Security hardening
[ ] Concurrent execution
[ ] Persistent storage
[ ] API layer
[ ] Observability
```

Technical debt should be addressed deliberately rather than hidden.

---

# 37. Important Linux Concepts to Master

Throughout the project, study:

## Processes

* process creation
* `fork`
* process image
* `exec`
* parent/child relationships
* exit status
* zombies
* orphans
* process groups
* sessions

## File descriptors

* descriptor tables
* `open`
* `close`
* `dup`
* `dup2`
* inheritance across fork
* close-on-exec

## Pipes

* pipe buffers
* blocking
* nonblocking I/O
* EOF
* pipe backpressure
* deadlocks

## Polling

* `poll`
* readiness
* blocking vs nonblocking
* multiplexing streams

## Signals

* signal delivery
* default actions
* signal handlers
* SIGTERM
* SIGKILL
* SIGCHLD
* SIGSEGV
* process groups

## Resource management

* CPU limits
* memory limits
* file limits
* process limits
* cgroups
* rlimits

---

# 38. Interview Preparation

Every major subsystem should eventually generate interview questions.

Examples:

### ProcessRunner

* Why `fork()` instead of `system()`?
* Why `execvp()`?
* Why are pipes needed?
* Why `dup2()`?
* What happens to file descriptors after `fork()`?
* Why can stdout/stderr pipes deadlock?
* Why use `poll()`?
* What happens when a pipe buffer fills?
* How do you detect abnormal termination?
* How do you avoid zombies?

### TestRunner

* Why dependency injection?
* Why interfaces?
* Why virtual functions?
* Why mocks?
* Why not instantiate ProcessRunner directly?
* Why fail fast?
* How would this scale to thousands of tests?

### Resource limits

* How would you enforce CPU time?
* How would you enforce memory?
* Why might `RLIMIT_*` be insufficient?
* How do cgroups differ from rlimits?

### Sandboxing

* Why is a timeout not a sandbox?
* How do namespaces work?
* What does seccomp protect against?
* How do containers isolate processes?

### Distributed execution

* What happens if a worker dies?
* How do you retry jobs?
* How do you avoid duplicate execution?
* How would you implement backpressure?

---

# 39. Resume-Level Outcomes

Eventually the project should demonstrate experience with:

* Linux process lifecycle
* POSIX IPC
* nonblocking I/O
* resource enforcement
* process isolation
* sandboxing
* C++20 architecture
* dependency injection
* unit and integration testing
* concurrency
* worker pools
* job queues
* REST APIs
* databases
* observability
* benchmarking
* distributed systems

Potential resume bullets will be generated only after the corresponding functionality is actually implemented and measured.

---

# 40. Milestone Roadmap

## M1 — Project Foundation

[x]

* C++20 project
* CMake
* Git
* basic architecture
* result models
* initial executable

---

## M2 — Linux Process Execution

[x]

* fork
* exec
* waitpid
* arguments
* stdout
* stderr
* stdin
* pipes
* file descriptors

---

## M3 — Compiler + Judge

[x]

* Compiler abstraction
* CompileResult
* Judge
* JudgeResult
* basic verdicts
* compilation pipeline

---

## M4 — Test Infrastructure

[x]

* TestCase
* TestSuite
* TestRunner
* multiple test execution
* fail-fast behavior

---

## M5 — Robust ProcessRunner

[x]

* streaming output
* polling
* large input/output fixtures
* signal-related fixtures
* extensive ProcessRunner tests

---

## M6 — Testability & Architecture

[x]

* IProcessRunner
* IJudge
* dependency injection
* MockProcessRunner
* MockJudge
* TestRunner unit tests
* GoogleTest
* GoogleMock

Compiler and SubmissionService tests intentionally deferred.

---

# M7 — Execution Platform

## Completed

[x] Standard input redirection

[x] Multiple test case execution

[x] Robust streaming output capture

## Remaining

[ ] Execution time measurement

[ ] Memory measurement

[ ] Time Limit Exceeded

[ ] Memory Limit Exceeded

[ ] Runtime signal handling

[ ] Large-output handling

[ ] Process cleanup and failure recovery

---

# M8 — Resource Enforcement & Security Foundations

Potential scope:

* rlimits
* process groups
* CPU enforcement
* memory enforcement
* process limits
* output limits
* cleanup
* isolation fundamentals

---

# M9 — Linux Sandbox

Potential scope:

* namespaces
* cgroups
* filesystem isolation
* seccomp
* restricted environment
* network restrictions

---

# M10 — Docker Execution Environment

Potential scope:

* Docker
* container lifecycle
* container resource limits
* image management
* secure container execution

---

# M11 — Backend API

Potential scope:

* REST API
* submission endpoint
* problem endpoint
* authentication
* request validation
* result retrieval

---

# M12 — Persistence

Potential scope:

* SQL
* schema design
* migrations
* submissions
* problems
* users
* results
* indexes
* transactions

---

# M13 — Concurrency

Potential scope:

* job queue
* worker pool
* bounded queues
* producer/consumer
* synchronization
* backpressure
* graceful shutdown

---

# M14 — Distributed Execution

Potential scope:

* worker registration
* scheduler
* job ownership
* retries
* failure detection
* idempotency
* horizontal scaling

---

# M15 — Observability

Potential scope:

* structured logs
* metrics
* tracing
* latency
* throughput
* CPU metrics
* memory metrics
* worker health

---

# M16 — Performance Engineering

Potential scope:

* benchmark framework
* reproducible workloads
* sequential vs concurrent execution
* P50/P95/P99
* throughput
* bottleneck identification
* profiling
* optimization

---

# M17 — Production Deployment

Potential scope:

* Docker Compose / deployment architecture
* CI/CD
* configuration management
* health checks
* monitoring
* failure recovery
* production documentation

---

# 41. Final Target Architecture

The eventual architecture should evolve toward:

```text
                         Client
                           |
                           v
                     REST API Server
                           |
                           v
                  Submission Service
                           |
              +------------+------------+
              |                         |
              v                         v
          Database                  Job Queue
                                        |
                                        v
                               +--------+--------+
                               |        |        |
                               v        v        v
                            Worker   Worker   Worker
                               |        |        |
                               v        v        v
                            Sandbox  Sandbox  Sandbox
                               |        |        |
                               +--------+--------+
                                        |
                                        v
                                   ProcessRunner
                                        |
                         +--------------+--------------+
                         |              |              |
                       stdin         stdout         stderr
                         |              |              |
                         +--------------+--------------+
                                        |
                                  Resource Monitor
                                  /      |       \
                               CPU     Memory    Time
                                |        |        |
                               TLE      MLE      TLE
                                        |
                                        v
                                  ExecutionResult
                                        |
                                        v
                                      Judge
                                        |
                                        v
                                  JudgeResult
                                        |
                                        v
                                  SubmissionResult
                                        |
                         +--------------+--------------+
                         |                             |
                         v                             v
                     Database                     API Client
```

---

# 42. Definition of Done

A feature is not considered complete merely because the code compiles.

A meaningful feature should satisfy:

```text
[ ] Requirements understood
[ ] Design alternatives considered
[ ] Architecture chosen
[ ] Implementation complete
[ ] Unit tests added where appropriate
[ ] Integration tests added where appropriate
[ ] Failure cases tested
[ ] Edge cases tested
[ ] Security implications considered
[ ] Concurrency implications considered
[ ] Performance implications considered
[ ] Existing tests still pass
[ ] Code reviewed
[ ] Technical debt documented
[ ] Git checkpoint created
[ ] Interview explanation prepared
```

---

# 43. Engineering Rules for Future Development

1. Do not add abstractions without a reason.
2. Do not couple high-level components to low-level implementations unnecessarily.
3. Do not use mocks where real integration behavior is what needs testing.
4. Do not use real external systems when testing pure orchestration logic.
5. Do not add production functionality merely to satisfy a test.
6. Do not optimize without measurement.
7. Do not assume Linux behavior—verify it experimentally.
8. Do not ignore failure paths.
9. Do not hide resource ownership.
10. Do not leave child processes unreaped.
11. Do not confuse resource limiting with security isolation.
12. Do not treat Docker as a complete sandbox by default.
13. Do not introduce concurrency before the sequential execution model is correct.
14. Do not introduce distributed systems before the single-worker system is reliable.
15. Keep components independently understandable.
16. Prefer explicit result modeling.
17. Keep security boundaries visible.
18. Keep tests focused on component responsibilities.
19. Make architectural decisions explainable in an interview.
20. Optimize for long-term engineering understanding.

---

# 44. Current State

As of the current milestone:

```text
                    ONLINE JUDGE

Project Foundation           ✅
CMake                        ✅
Git                          ✅

ProcessRunner                ✅
  fork                       ✅
  exec                       ✅
  stdin                      ✅
  stdout                     ✅
  stderr                     ✅
  argument passing           ✅
  streaming/polling          ✅
  large I/O fixtures         ✅

TestSuite                    ✅
Judge                        ✅
Compiler integration         ✅
SubmissionService            ✅
TestRunner                   ✅

Dependency Injection         ✅
IProcessRunner               ✅
IJudge                       ✅

MockProcessRunner            ✅
MockJudge                    ✅
TestRunner unit tests        ✅

Compiler tests               ⏸ deferred
SubmissionService tests      ⏸ deferred

M7 Execution Platform

stdin redirection            ✅
multiple test cases          ✅
streaming output             ✅
execution time               ⏳
memory measurement           ⏳
TLE                          ⏳
MLE                          ⏳
signal classification        ⏳
large-output policy          ⏳
cleanup/recovery             ⏳
```

---

# 45. Immediate Next Step

The immediate task is:

## M7 — Execution Time Measurement

Before implementation, determine:

1. What exactly should execution time represent?
2. Which clock should be used?
3. When should measurement begin?
4. When should measurement end?
5. Should fork/exec overhead be included?
6. How will the value be stored in `ExecutionResult`?
7. How will tests verify it without becoming flaky?
8. How will the measurement eventually support TLE?

Only after answering these questions should implementation begin.

---

# 46. Long-Term Goal

The final goal is not merely:

> "I built an online judge."

The goal is to be able to confidently explain:

> How an untrusted program moves from source code to compilation, process creation, execution, IPC, output capture, resource monitoring, failure handling, sandboxing, judging, persistence, queuing, concurrent execution, and eventually distributed execution.

Every major subsystem should be understandable from first principles.

The project should demonstrate not only that the system works, but that its author understands:

* why it works
* why the architecture looks the way it does
* what can fail
* how it scales
* how it can be attacked
* how it can be measured
* how it can be tested
* and how it can evolve into a production system.

````

### One important change from your old README

I deliberately made **TestRunner a first-class architectural component**. Your old README's pipeline jumped from `ProcessRunner → Judge`, but the actual project now has:

```text
SubmissionService
      ↓
Compiler
      ↓
TestRunner
   ↙      ↘
ProcessRunner  Judge
````

That matters because we've now established a genuine dependency-injection boundary around TestRunner.

I also explicitly recorded that **Compiler and SubmissionService testing are deferred until after M7**, rather than treating those as forgotten TODOs. That's an intentional engineering decision.

### Current position

We're now here:

```text
M1 Foundation             ✅
M2 Process execution      ✅
M3 Compiler + Judge       ✅
M4 Test infrastructure    ✅
M5 Robust ProcessRunner   ✅
M6 Testability            ✅
                          │
                          ▼
                     YOU ARE HERE
                          │
                          ▼
M7 Execution Platform     🚧
   ├─ time measurement
   ├─ memory measurement
   ├─ TLE
   ├─ MLE
   ├─ signal handling
   ├─ output limits
   └─ cleanup/recovery
```

**M7 is the next major engineering phase.** We should not jump directly into code. The first thing we should reason through is **execution-time measurement**, then implement it and test it in the existing ProcessRunner architecture.

Event-Driven Real-Time Telemetry

The eventual distributed system should support real-time submission lifecycle events, rather than requiring clients to repeatedly poll for status.

The high-level flow becomes:

                         Client
                           │
                WebSocket / SSE / gRPC
                           │
                           ▼
                  Submission Gateway
                           │
                           ▼
                  SubmissionService
                           │
                           ▼
                       Job Queue
                           │
                           ▼
                     Worker
                           │
              ┌────────────┴────────────┐
              │                         │
              ▼                         ▼
         Execution                 Event Publisher
          Sandbox                       │
              │                         │
              │                         ▼
              │                   Message Broker
              │                         │
              │                ┌────────┴────────┐
              │                │                 │
              ▼                ▼                 ▼
          TestRunner       Event Service       Logging
              │                │
              │                ▼
              │          Connected Clients
              │
              ▼
         Final Result
Example event stream

A submission could produce events such as:

SubmissionQueued
      ↓
CompilationStarted
      ↓
CompilationFinished
      ↓
ExecutionStarted
      ↓
TestCaseStarted { test: 1 }
      ↓
TestCaseFinished {
    test: 1,
    verdict: Accepted
}
      ↓
TestCaseStarted { test: 2 }
      ↓
TestCaseFinished {
    test: 2,
    verdict: Accepted
}
      ↓
SubmissionFinished {
    verdict: Accepted
}

For a failure:

CompilationStarted
      ↓
CompilationFinished
      ↓
ExecutionStarted
      ↓
TestCaseStarted { test: 1 }
      ↓
TestCaseFinished {
    test: 1,
    verdict: Accepted
}
      ↓
TestCaseStarted { test: 2 }
      ↓
TestCaseFinished {
    test: 2,
    verdict: WrongAnswer
}
      ↓
SubmissionFinished {
    verdict: WrongAnswer
}
Why this is architecturally important

We should not do this:

ProcessRunner
      ↓
WebSocket
      ↓
Browser

That would tightly couple our low-level execution engine to the UI.

Instead:

ProcessRunner
      ↓
Execution facts/events
      ↓
Worker
      ↓
Event Publisher
      ↓
Message Broker
      ↓
Event consumers
      ↓
Clients / logging / monitoring

This preserves our existing separation of concerns.

ProcessRunner remains concerned with:

executing processes safely and collecting execution information.

It doesn't know:

who the user is
which browser is connected
WebSockets
REST
UI state
message brokers
Event model

Eventually we'll want a domain-level event model along the lines of:

SubmissionEvent {
    submission_id;
    event_type;
    timestamp;
    test_index;
    metadata;
};

Possible event types:

SubmissionQueued
CompilationStarted
CompilationFinished
ExecutionStarted
TestCaseStarted
TestCaseFinished
ResourceLimitExceeded
ExecutionTerminated
SubmissionFinished
SubmissionFailed

This will also give us a natural foundation for observability.

The same event stream can feed:

                 Submission Events
                       │
        ┌──────────────┼──────────────┐
        ↓              ↓              ↓
      Client        Metrics        Logs
        │              │              │
        ↓              ↓              ↓
      UI          monitoring       debugging

That's much more powerful than building telemetry specifically for the frontend.

Where it belongs in the roadmap

I'd modify the later roadmap to:

M11 — Backend API
    [ ] REST API
    [ ] Submission API
    [ ] Authentication
    [ ] Request validation


M12 — Persistence
    [ ] Database
    [ ] Submission persistence
    [ ] Result persistence
    [ ] Problem management


M13 — Event-Driven Architecture
    [ ] Submission lifecycle events
    [ ] Event model
    [ ] Event publisher
    [ ] Message broker
    [ ] Event consumers
    [ ] Event ordering
    [ ] Event delivery semantics


M14 — Real-Time Client Updates
    [ ] WebSocket / SSE / gRPC streaming
    [ ] Submission progress streaming
    [ ] Test-case progress
    [ ] Resource telemetry
    [ ] Connection recovery
    [ ] Client subscription management


M15 — Concurrency
    [ ] Submission queue
    [ ] Worker pool
    [ ] Concurrent execution
    [ ] Backpressure
    [ ] Worker failure handling


M16 — Distributed Execution
    [ ] Worker registration
    [ ] Scheduler
    [ ] Worker health
    [ ] Job ownership
    [ ] Retries
    [ ] Idempotency
    [ ] Horizontal scaling


M17 — Observability
    [ ] Structured logging
    [ ] Metrics
    [ ] Tracing
    [ ] Submission latency
    [ ] Throughput
    [ ] P50/P95/P99

I'd actually consider event-driven architecture a cross-cutting capability, rather than merely a frontend feature. The events can eventually power UI updates, monitoring, auditing, metrics, and debugging simultaneously.

One thing I would not commit to yet

You mentioned:

WebSockets or gRPC streams

Let's keep that as an architectural option, not a decision.

Later we'll compare:

WebSocket
SSE
gRPC streaming

based on:

bidirectional vs unidirectional communication
browser support
connection management
scalability
proxies/load balancers
protocol complexity
internal vs external communication

Similarly, we shouldn't choose Kafka/RabbitMQ/NATS/etc. yet.

The important decision now is:

Submission progress should be represented as events, and event production should be decoupled from event consumption.

The concrete transport can be selected when we reach that milestone.

So our long-term architecture now becomes
                     ┌─────────────────┐
                     │      Client     │
                     └────────┬────────┘
                              │
                    REST + Real-time stream
                              │
                              ▼
                     Submission API
                              │
                              ▼
                    SubmissionService
                       /            \
                      /              \
                     ▼                ▼
                Database          Job Queue
                                      │
                                      ▼
                                  Workers
                                      │
                                      ▼
                                   Sandbox
                                      │
                                      ▼
                                TestRunner
                                /        \
                               ▼          ▼
                        ProcessRunner    Judge
                               │
                               ▼
                        ExecutionResult
                                     
                    Worker emits domain events
                              │
                              ▼
                       Event Publisher
                              │
                              ▼
                        Message Broker
                         /     |      \
                        ▼      ▼       ▼
                     Client  Metrics  Logs

I'll treat this event-driven telemetry architecture as part of the project's long-term design from this point forward. It fits very naturally with the worker/distributed-systems phase we're building toward.