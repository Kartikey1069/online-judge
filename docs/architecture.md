Online Judge — Current Architecture

This document describes the architecture that exists at the completion of M9 and M10. It is an architectural reference, not a development diary.

1. System Boundaries

The system has three important execution boundaries:

Application / Domain Layer
        |
        v
Execution Infrastructure
        |
        v
Sandbox / Linux Isolation

At the application level:

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

The execution infrastructure is intentionally separated from judging semantics.

ProcessRunner

Owns:

process lifecycle

process creation

executable replacement

stdin/stdout/stderr

polling

execution monitoring

resource measurement

termination

reaping

sandbox coordination

execution facts

It does not know what Accepted or Wrong Answer means.

TestRunner

Owns:

iterating through test cases

passing input to execution

forwarding execution results to Judge

fail-fast behavior

preserving failed-test information

Judge

Owns:

interpreting execution facts

comparing execution output with expected output

producing logical verdicts

SubmissionService

Owns high-level orchestration and should not duplicate the responsibilities of Compiler, TestRunner, ProcessRunner, or Judge.

2. ProcessRunner Execution Model

The execution engine creates three standard I/O channels and a control channel.

ProcessRunner
    |
    +---- stdin ------------------> child
    |
    +<--- stdout ------------------ child
    |
    +<--- stderr ------------------ child
    |
    +<--- control/status --------- sandbox infrastructure

The parent and child inherit file descriptors across fork(), so each process closes descriptors it does not own immediately after the fork.

This is required both for resource hygiene and for correct EOF behavior.

Nonblocking execution loop

The parent multiplexes output/control activity instead of blocking on one stream.

poll()
  |
  +--> stdin writable
  +--> stdout readable
  +--> stderr readable
  +--> control/status readable
  |
  v
read/write incrementally
  |
  v
check process state
  |
  v
repeat until lifecycle completion

This prevents stdout or stderr pipe buffers from becoming a process-level deadlock.

The execution loop also performs periodic nonblocking process checks when no I/O descriptor is available to wake poll().

3. Supervisor Boundary

The sandbox introduces a dedicated Supervisor between ProcessRunner and the untrusted execution workload.

ProcessRunner
      |
      | control channel
      v
Supervisor
      |
      | manages
      v
Submission
      |
      +--> descendants

The Supervisor exists to isolate sandbox lifecycle responsibilities from the external ProcessRunner.

The critical containment relationship is:

Supervisor
    = controller

Submission + descendants
    = workload

Execution cgroup
    = containment boundary

The Supervisor is intentionally outside the execution cgroup.

If the Supervisor were inside the cgroup, a cgroup.kill operation could kill the controller along with the workload.

4. Cgroup Architecture

Each execution receives a dedicated cgroup under the Online Judge cgroup hierarchy.

Conceptually:

/sys/fs/cgroup/
    |
    +-- online-judge/
          |
          +-- execution-<id>/

The execution ID is used for:

cgroup identity

lifecycle correlation

cleanup ownership

diagnostics

The Supervisor creates the execution cgroup and the actual Submission host PID is written to cgroup.procs.

Synchronization invariant

The Submission must not continue until:

fork()
  |
  v
child waits
  |
  v
parent writes child PID to cgroup.procs
  |
  v
parent releases child
  |
  v
child continues sandbox setup

This is a deliberate happens-before relationship.

Kill vs cleanup

These are different operations.

cgroup.kill
    = terminate the workload

cgroup cleanup
    = remove the execution cgroup after the workload is gone

The system must never treat them as the same lifecycle operation.

5. Sandbox Namespace Architecture

The Submission constructs its isolation environment after it has been placed into the cgroup.

User namespace

The child:

captures the host UID/GID

calls unshare(CLONE_NEWUSER)

configures /proc/self/setgroups

writes UID/GID maps

The host credentials must be captured before entering the new user namespace.

Seeing an unmapped UID/GID immediately after unshare() is expected until the maps are installed.

Mount namespace

The child creates:

CLONE_NEWNS

and then makes the root mount private:

MS_REC | MS_PRIVATE

This prevents mount propagation between the sandbox and the host.

Temporary root

The Supervisor creates a temporary root directory under /tmp.

The Submission then bind-mounts that root from inside its mount namespace.

The bind mount belongs to the sandbox's mount namespace rather than being performed as an ordinary unprivileged host operation.

PID namespace

The child enters:

CLONE_NEWPID

The semantics are important: the process that calls unshare(CLONE_NEWPID) does not itself become PID 1. A subsequent child becomes PID 1 in the new PID namespace.

Therefore the system distinguishes:

host PID
namespace PID

Filesystem finalization

The sandbox performs:

mkdir(old_root)
pivot_root(new_root, old_root)
chdir("/")
umount2("/old_root", MNT_DETACH)
rmdir("/old_root")

Then it creates and mounts /proc.

Required runtime directories/libraries and the executable are exposed through controlled bind mounts.

6. Privilege Model

Current development model

Some sandbox operations require elevated host privileges in the current WSL development environment.

Running the Supervisor/judge with sudo is a development/testing mechanism.

It does not mean the submitted program should run as host root.

Long-term production model

The intended production boundary is:

API Server
    |
    | unprivileged IPC request
    v
Privileged Sandbox Supervisor
    |
    v
Isolated Submission

The API server should not run as root.

The Supervisor should expose a narrow execution interface rather than arbitrary privileged operations.

A future IPC contract can conceptually expose:

CreateExecution
RunExecution
GetExecutionStatus
TerminateExecution

rather than operations such as arbitrary mount, arbitrary PID kill, or arbitrary cgroup-file writes.

Capability minimization

Full root is not the final objective.

The eventual hardening process should identify the exact Linux capabilities required by each privileged operation and remove unnecessary privilege.

This is deliberately deferred beyond M10.

7. Lifecycle State Model

The execution lifecycle is modeled conceptually as:

CREATED
    |
CGROUP_CREATED
    |
ROOT_CREATED
    |
CHILD_CREATED
    |
CHILD_IN_CGROUP
    |
CHILD_RELEASED
    |
ISOLATION_INITIALIZED
    |
FILESYSTEM_READY
    |
EXECUTING
    |
FINISHED
    |
CLEANUP
    |
CLEANED

Not every implementation detail needs to be represented as a literal enum, but resource ownership must correspond to lifecycle state.

The fundamental rule is:

Once a resource is successfully created, the system owns the obligation to clean it up even if the next operation fails.

8. Failure and Recovery

Failure is treated as part of the architecture.

Examples:

cgroup creation fails
root creation fails
fork fails
cgroup attachment fails
user namespace setup fails
mount setup fails
pivot_root fails
exec fails
timeout occurs
Supervisor stops responding
cleanup itself fails

A failure after partial setup must roll back resources already created.

Conceptually:

A created
  |
B created
  |
C fails
  |
cleanup B
  |
cleanup A

Supervisor failure

Supervisor death does not prove that the Submission is gone.

The stronger recovery boundary is:

Supervisor failure
        |
        v
contain/kill execution cgroup
        |
        v
verify workload termination
        |
        v
kill/reap Supervisor if necessary
        |
        v
cleanup execution resources

The cgroup therefore provides stronger containment than relying only on the parent-child relationship.

PDEATHSIG

PDEATHSIG remains optional/deferred.

It is not the primary containment mechanism.

9. IPC Protocol

Sandbox lifecycle communication is structured.

The protocol includes message types conceptually equivalent to:

enum class SandboxMessageType {
    Ready,
    SetupFailed,
    Terminated,
    Terminate
};

Structured payloads carry:

Ready

host PID

namespace PID

execution ID

SetupFailed

setup stage

errno

Terminated

wait status

resource usage

Terminate

no payload

The protocol uses a message header containing message type and payload size.

Reads and writes must account for partial I/O and EINTR.

The status channel distinguishes:

Payload
EOF
Error

EOF is not automatically equivalent to a valid termination message.

10. Execution Result Model

The low-level execution subsystem reports facts.

Conceptually:

ExecutionResult
├── status
├── exit information
├── stdout
├── stderr
├── execution time
├── memory usage
└── termination information

Execution status distinguishes infrastructure and workload outcomes.

Examples:

Completed
TimedOut
Signaled
SandboxFailure
RunnerFailure

A sandbox setup failure can additionally identify:

SandboxSetupStage
errno

This prevents an infrastructure failure such as a failed mount from being confused with a user program that simply exited with a non-zero status.

11. Cleanup Invariants

A completed execution should satisfy:

no submission process remains
no unintended descendant remains
every child is reaped
execution FDs are closed
execution cgroup is cleaned
temporary execution resources are cleaned
partial setup does not leak resources

Cleanup failure must remain observable.

An execution result of "completed successfully" must not hide an infrastructure cleanup failure.

This matters particularly for long-lived workers where small leaks become systemic failures.

12. Testing Architecture

Testing is separated by responsibility.

ProcessRunner

Use real Linux fixture programs.

Reason: the important behavior is interaction with the kernel, processes, pipes, descriptors, signals, and timing.

TestRunner

Use mocks.

Reason: the important behavior is orchestration and dependency interaction.

Judge

Use deterministic unit tests.

Sandbox

Use Linux integration tests and host-aware tests where kernel privileges affect availability.

Failure injection

The test strategy intentionally exercises failures such as:

fork failure
exec failure
pipe failure
read/write failure
poll failure
timeout
signal termination
cgroup failure
cleanup failure
Supervisor failure

Tests should validate behavior rather than private implementation details.

13. Current Validation

The current WSL environment has validated ordinary runner behavior.

Latest reported validation:

ProcessRunner suite
    12 tests run
    11 passed
    1 skipped

Repository CTest
    1/1 test suite passed
    0 failed

The skipped test is the privileged signal-termination test.

It is gated on an actual capability check for writing to cgroup.kill; it is not treated as a normal runtime failure.

The current WSL user cannot directly write to the prepared cgroup control files, and sudo is not usable without a password. The full privileged signal path therefore still requires a root-capable host or privileged environment.

14. M10 Freeze Boundary

M10 establishes a stable local execution-engine boundary.

Later components should communicate with the execution layer through domain-level concepts:

ExecutionRequest
ExecutionResult
SubmissionResult

rather than reaching into:

cgroup
namespace
mount
PID
FD

details.

The future backend should not know how pivot_root() or cgroup.kill works.

15. Future Backend Integration

The intended architecture after the engine freeze is:

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
Worker
  |
  v
Execution Engine
  |
  v
Sandbox Supervisor
  |
  v
Submission

The privileged Supervisor can eventually become a separate local service/process accessed over a Unix domain socket.

The important boundary is:

Backend/domain logic
        |
        | narrow execution interface
        v
Sandbox supervisor
        |
        v
Linux primitives

This keeps the privileged Linux surface small and makes the backend easier to reason about and secure.

16. Future Event Architecture

Execution should eventually produce domain-level events such as:

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

The execution engine should not talk directly to browsers.

Instead:

Worker
  |
  v
Event Publisher
  |
  v
Message Broker
  |
  +--> client updates
  +--> metrics
  +--> logs

The eventual real-time transport may be WebSocket, SSE, or gRPC streaming; no final choice is made yet.

Likewise, no message broker choice is fixed yet.

The architectural decision is simply that event production should be decoupled from event consumption.

17. Future Scaling

After the single-execution engine is stable:

API
 |
 v
Job Queue
 |
 +---- Worker A ---- Sandbox
 |
 +---- Worker B ---- Sandbox
 |
 +---- Worker C ---- Sandbox

Future concerns include:

bounded queues

backpressure

worker failure

cancellation

graceful shutdown

scheduling

job ownership

retries

idempotency

horizontal scaling

Concurrency and distribution are intentionally layered on top of the now-stable execution boundary.

18. Architectural Invariants

These are the most important current rules.

ProcessRunner reports execution facts; Judge determines semantic verdicts.

Every created child has a reaping path.

Every execution FD has a known owner.

Unused FD copies are closed after fork().

stdout and stderr are drained concurrently.

The Supervisor remains outside the execution cgroup.

The Submission and descendants are contained by the execution cgroup.

Cgroup kill and cgroup cleanup are separate operations.

Partial setup creates cleanup obligations.

Cleanup failures are not silently converted to success.

Supervisor failure does not imply workload cleanup is complete.

Strong workload containment is preferred over relying only on parent-child relationships.

Low-level execution code does not contain judging/business semantics.

Backend/API code must not directly control raw sandbox primitives.

Privileged operations should eventually live behind a narrow Supervisor boundary.

19. Architectural Decisions

ProcessRunner vs Judge

Execution and semantic judging are separate responsibilities.

Supervisor outside cgroup

The controller must remain outside the workload containment boundary.

Cgroup as containment boundary

Single-PID termination is insufficient for arbitrary descendants.

Structured sandbox protocol

Explicit messages are more reliable than inferring every state from process exit.

Explicit execution identity

Execution IDs correlate sandbox resources and lifecycle events.

Namespace-based isolation

Linux primitives are implemented and understood directly before introducing container abstractions.

Privilege separation

The current privileged development environment is not the final backend architecture.

Event-driven future

Execution progress should eventually become domain events rather than direct UI communication.