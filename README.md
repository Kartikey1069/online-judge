Online Judge

A production-oriented Online Judge built from scratch in Modern C++ on Linux.

The project focuses on the engineering behind code execution rather than only the judging logic. It currently includes a Linux process-execution engine, resource-aware execution, a namespace/cgroup-based sandbox, structured execution results, and automated tests for process and lifecycle behavior.

Features

C++20 and CMake

Linux process execution with fork() and exec*()

stdin/stdout/stderr redirection through pipes

nonblocking I/O and poll()-based stream multiplexing

large input/output handling

binary output preservation

process exit and signal handling

wall-clock and CPU-time measurement

CPU/time resource enforcement

memory measurement/enforcement foundations

explicit execution-limit configuration

process-group/cgroup-based containment

Linux user, mount, and PID namespaces

filesystem isolation using a temporary root and pivot_root()

/proc inside the execution environment

dedicated sandbox Supervisor

structured sandbox IPC

execution IDs for lifecycle/resource correlation

explicit sandbox setup failure stages

bounded cleanup and failure recovery

child reaping and zombie prevention

dependency inversion through IProcessRunner and IJudge

GoogleTest/GoogleMock-based testing

Architecture

The application-level execution pipeline is:

SubmissionService
        |
        v
     Compiler
        |
        v
    TestRunner
      /     \
     v       v
ProcessRunner Judge
     |
     v
ExecutionResult
     |
     v
 JudgeResult
     |
     v
SubmissionResult

ProcessRunner is responsible for execution facts: process lifecycle, I/O, timing, resource information, signals, and failures.

Judge is responsible for interpreting those facts and deciding the logical verdict.

TestRunner coordinates execution against multiple test cases and stops at the first non-accepted result.

Sandbox

Untrusted code is not executed directly by the application process.

The execution path uses a dedicated Supervisor and a Linux sandbox:

ProcessRunner
      |
      v
  Supervisor
      |
      v
  Submission
      |
      +--> descendants
      |
      v
Execution cgroup

The Supervisor is deliberately outside the execution cgroup.

The Submission and its descendants are placed inside the execution cgroup.

This gives the system a controller/containment separation:

Supervisor       = controller
Execution cgroup = containment boundary
Submission       = untrusted workload

The child constructs its isolation environment using Linux namespaces and filesystem isolation. The current sandbox includes:

user namespace and UID/GID mapping

mount namespace

private mount propagation

temporary execution root

bind mounts for the required runtime environment

PID namespace

pivot_root()

/proc

process/resource limits

execve() of the submitted executable

The parent uses a synchronization mechanism so the Submission cannot continue until its host PID has successfully been added to the execution cgroup.

Process Execution

The execution engine uses explicit Linux process primitives instead of shell-based process launching.

fork()
  |
  +---- child
  |      |
  |      +-- configure descriptors
  |      +-- sandbox setup
  |      +-- exec()
  |
  +---- parent
         |
         +-- write input
         +-- poll stdout/stderr/control
         +-- monitor process
         +-- collect result
         +-- cleanup

stdout and stderr are drained concurrently. This prevents a child from blocking when one pipe fills while the parent is waiting on another stream.

Results

Execution and judging are represented separately.

An execution result contains facts such as:

exit information

stdout

stderr

execution time

memory usage

termination information

execution status

Execution status distinguishes conditions such as:

Completed
TimedOut
Signaled
SandboxFailure
RunnerFailure

Sandbox setup failures also identify the stage and underlying error code.

The Judge converts execution facts into a semantic verdict such as Accepted or Wrong Answer.

Testing

The project uses different testing strategies for different responsibilities.

ProcessRunner

Uses real Linux fixture programs to test:

process creation

stdin

stdout

stderr

large I/O

signals

process termination

lifecycle behavior

sandbox coordination

TestRunner

Uses mocks for IProcessRunner and IJudge to test orchestration and fail-fast behavior without launching real processes.

Judge

Uses deterministic unit tests.

Current validation

The current WSL development environment has been validated with:

ProcessRunner suite: 12 tests run, 11 passed, 1 skipped

repository CTest: 1/1 test suite passed

the skipped test is the privileged signal-termination case and is gated on whether the host can perform the required real cgroup.kill write

The privileged signal path still needs validation on a root-capable host or privileged environment.

Development

The project is built with CMake.

Typical development flow:

cmake -S . -B build
cmake --build build
ctest --test-dir build

The exact executable/test target names are defined by the repository's CMake configuration.

Sandbox tests may require elevated privileges depending on the host's cgroup and namespace configuration. The current development setup may therefore use a root-capable WSL shell or equivalent privileged environment when required.

The submitted program itself is never intended to be launched through sudo.

Project Status

The current milestone progression is:

M1  Project Foundation              ✅
M2  Linux Process Execution         ✅
M3  Compiler + Judge                ✅
M4  Test Infrastructure             ✅
M5  Robust ProcessRunner             ✅
M6  Testability & Dependency Inversion ✅
M7  Execution Engine                 ✅
M8  Resource & Security Foundations  ✅
M9  Linux Sandbox                    ✅
M10 Engine Testing & Hardening       ✅

M10 establishes a freeze point for the local execution engine. Later backend work should build on this execution boundary rather than mixing HTTP/API responsibilities into the low-level process runner.

Future Direction

The planned system evolves toward:

Client
  |
  v
REST API
  |
  v
SubmissionService
  |
  +---------> Database
  |
  v
Job Queue
  |
  v
Workers
  |
  v
Sandbox
  |
  v
TestRunner
 /       \
v         v
ProcessRunner  Judge

Later milestones will cover:

backend API

persistence

event-driven execution updates

real-time client updates

concurrent workers

job queues and backpressure

distributed execution

observability

performance engineering

production deployment

The execution layer remains independent of HTTP, browsers, databases, and event transports.