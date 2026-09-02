Online Judge — Canonical Engineering Context

1. Project Goal

This project is a production-oriented Online Judge built from scratch in Modern C++ on Linux.

The project is intentionally being used to develop strong systems-engineering skills in:

Linux internals

Operating systems

process management

POSIX IPC

file descriptors

resource management

sandboxing

security

C++ architecture

backend engineering

databases

concurrency

distributed systems

observability

performance engineering

system design

The objective is not just to have a working judge. The objective is to understand why every subsystem exists, what can fail, how it is isolated, how it scales, and how to defend its design in an engineering interview.

2. Current Checkpoint

M1 through M10 are complete.

M1  Project Foundation                 ✅
M2  Linux Process Execution            ✅
M3  Compiler + Judge                   ✅
M4  Test Infrastructure                ✅
M5  Robust ProcessRunner               ✅
M6  Testability / Dependency Inversion ✅
M7  Execution Engine                   ✅
M8  Resource & Security Foundations    ✅
M9  Linux Sandbox                      ✅
M10 Engine Testing & Hardening         ✅

The current execution engine is treated as a stable subsystem.

The next major architectural direction is the backend/API layer.

3. Current High-Level Architecture

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

Sandbox infrastructure sits below the execution path:

ProcessRunner
      |
      v
Supervisor
      |
      v
Submission
      |
      v
Execution cgroup

The application layer and Linux isolation layer are deliberately separate.

4. Component Responsibilities

ProcessRunner

ProcessRunner is low-level execution infrastructure.

It owns:

process creation

fork()

exec*()

stdin/stdout/stderr

pipes

file descriptors

nonblocking I/O

poll()

process monitoring

timing

resource information

signals

process termination

reaping

sandbox coordination

execution facts

It must not know:

Accepted

Wrong Answer

problem semantics

testcase correctness

user-facing submission policy

Judge

Judge consumes execution facts and produces a semantic verdict.

TestRunner

TestRunner coordinates:

testcase iteration

execution

judging

fail-fast behavior

failed-test result propagation

It depends on:

IProcessRunner
IJudge

Compiler

Compiler orchestrates compilation and returns compilation information.

SubmissionService

SubmissionService coordinates the high-level pipeline.

It should not become another ProcessRunner, Compiler, or Judge.

5. Linux Process Foundation

The project explicitly uses:

fork()
exec*()
waitpid()
wait4()
pipe()
dup2()
poll()
read()
write()
close()
signals

The key mental model is:

fork()
    = create a process

exec()
    = replace the process image

The parent retains control over the lifecycle and descriptors while the child prepares itself for execution.

6. I/O Model

The execution engine has separate channels for:

stdin
stdout
stderr

The parent writes stdin and reads stdout/stderr.

The streams are handled concurrently.

The reason is pipe backpressure.

A child can block if a pipe becomes full.

Therefore the parent must not simply block waiting for one stream while another stream can fill.

The engine uses polling/nonblocking-style event-loop behavior and incremental reads/writes.

Important invariants:

large stdin must not deadlock

large stdout must not deadlock

large stderr must not deadlock

EOF must be detected correctly

unused descriptors must be closed

partial reads/writes must be handled

EINTR must be handled where appropriate

7. IPC Architecture

The sandbox protocol uses structured messages.

Current conceptual message types:

enum class SandboxMessageType {
    Ready,
    SetupFailed,
    Terminated,
    Terminate
};

Structured payloads include:

Ready

host_pid
namespace_pid
execution_id

SetupFailed

failed_stage
error_code

Terminated

wait_status
resource usage

Terminate

No payload.

The protocol uses a message header containing:

message type
payload size

Protocol utilities handle complete reads/writes rather than assuming one system call transfers an entire message.

8. Sandbox Setup

The sandbox is built around Linux namespaces, cgroups, and filesystem isolation.

Current child-side sequence is conceptually:

wait for parent release
        |
        v
user namespace
        |
        v
UID/GID mapping
        |
        v
mount namespace
        |
        v
make mounts private
        |
        v
bind temporary root
        |
        v
PID namespace
        |
        v
filesystem finalization
        |
        v
/proc
        |
        v
resource limits
        |
        v
exec

The ordering is intentional.

9. User Namespace

The host UID/GID must be captured before:

unshare(CLONE_NEWUSER)

The child then configures:

/proc/self/setgroups
/proc/self/uid_map
/proc/self/gid_map

The child initially has unmapped credentials immediately after entering the user namespace. That temporary state is expected until mappings are established.

The purpose is to provide namespace-local identity/capability semantics without simply making the untrusted program host root.

10. Mount Namespace

The child enters:

CLONE_NEWNS

and then:

MS_REC | MS_PRIVATE

on /.

The private propagation step is important because otherwise mount events can propagate between namespaces.

The sandbox then performs the bind mount of the temporary root inside the namespace.

This is important because the earlier host-side bind mount attempted as an ordinary user failed with EPERM. The correct solution was not to move all sandbox operations into a permanently privileged parent; the child receives the namespace-local privileges required for its isolation setup.

11. PID Namespace

The child enters:

CLONE_NEWPID

A critical Linux semantic:

The caller of unshare(CLONE_NEWPID) does not itself become PID 1.

A subsequently created child becomes PID 1 in the new namespace.

Therefore the project keeps both:

host PID
namespace PID

The host PID is used for host-side process management.

The namespace PID is useful for reasoning about the isolated process tree.

12. Filesystem Isolation

Each execution gets a temporary root similar to:

/tmp/oj-sandbox-XXXXXX

The sandbox bind-mounts it and then performs:

pivot_root()
chdir("/")
umount2("/old_root", MNT_DETACH)
rmdir("/old_root")

It then creates and mounts /proc.

Required runtime directories/libraries and the executable are exposed through controlled bind mounts.

The execution root is an execution resource and must have an explicit lifetime and cleanup path.

13. Cgroup Architecture

The system uses cgroup v2.

The execution hierarchy is conceptually:

/sys/fs/cgroup/
    |
    +-- online-judge/
          |
          +-- execution-<execution_id>/

The Supervisor creates the execution cgroup.

The Submission host PID is added to:

cgroup.procs

before the child is released.

Critical synchronization invariant

fork
  |
child waits
  |
parent adds child to cgroup
  |
parent releases child
  |
child continues

The Submission must never be allowed to continue into the execution path before containment has been established.

14. Supervisor / Cgroup Ownership

The Supervisor is deliberately outside the execution cgroup.

Correct:

ProcessRunner
    |
    +--> Supervisor          OUTSIDE cgroup
              |
              +--> Submission INSIDE cgroup
                       |
                       +--> descendants

Incorrect:

Execution cgroup
    |
    +--> Supervisor
    +--> Submission

The reason is controller/containment separation.

The Supervisor must remain alive long enough to control and clean the execution cgroup.

15. Cgroup Kill vs Cleanup

These are different lifecycle operations.

cgroup.kill
    = terminate processes in the workload boundary

cleanupCgroup()
    = destroy/remove the execution cgroup after workload termination

Therefore:

kill != cleanup

A cgroup should not be removed while the workload is still present.

16. Execution Identity

Every execution has an execution_id.

It is used for:

cgroup naming

lifecycle correlation

cleanup ownership

diagnostics

future logs/events

distinguishing concurrent executions

The identity should remain stable for the lifetime of an execution.

17. Execution Status

The execution result distinguishes infrastructure and workload outcomes.

Current status concepts include:

Completed
TimedOut
Signaled
SandboxFailure
RunnerFailure

A sandbox setup failure also identifies its stage.

Current setup stages include concepts such as:

Namespace
Filesystem
Identity
Cgroup
Capabilities
Seccomp
ProcessCreation
StandardIO
ResourceLimit
Exec

The exact enum should remain aligned with the repository.

18. Lifecycle Model

The conceptual lifecycle is:

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

The implementation does not need to expose every state publicly.

The important principle is resource ownership:

Every successfully created resource creates a cleanup obligation.

19. Failure Handling

The system treats failure paths as first-class behavior.

Failures include:

cgroup creation

root creation

pipe creation

fork

cgroup attachment

user namespace setup

mount namespace setup

mount

pivot root

/proc

resource setup

exec

polling

reads/writes

timeout

signal termination

Supervisor failure

cleanup failure

Partial setup must roll back already-created resources.

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

20. Supervisor Failure Recovery

The system does not assume that Supervisor death means the Submission is gone.

The recovery model is:

Supervisor failure
        |
        v
kill/contain execution cgroup
        |
        v
verify workload termination
        |
        v
kill/reap Supervisor if necessary
        |
        v
cleanup execution resources

This is stronger than relying only on parent-death relationships.

PDEATHSIG remains optional/deferred.

21. Reaping

Every child must have a reaping path.

A terminated child that is not waited for becomes a zombie.

The project therefore treats:

fork()
  |
child exits
  |
waitpid()/wait4()
  |
reaped

as a lifecycle invariant.

WNOHANG is used where nonblocking process observation is required.

A final blocking wait is appropriate when lifecycle completion must be established.

22. File Descriptor Ownership

Descriptors are process resources with explicit ownership.

The architecture includes channels such as:

ProcessRunner
├── stdin
├── stdout
├── stderr
└── control

Supervisor
├── execution I/O
├── control
└── status

Submission
├── stdin
├── stdout
├── stderr
└── status

Exact descriptor numbers are implementation details.

The invariant is:

created
  |
fork
  |
ownership determined
  |
unused copy closed
  |
normal operation
  |
normal/failure close

Leaked descriptors can delay EOF and cause deadlocks.

23. Cleanup

Cleanup is part of correctness.

A successful execution is not enough if resources remain behind.

The intended cleanup responsibilities cover:

child process

descendants

cgroup

temporary root

mounts

pipes

sockets

status/control descriptors

Cleanup errors must remain observable.

A cleanup failure must not silently become a successful infrastructure result.

24. Privilege Model

Current development

The Linux sandbox may require elevated host privileges depending on the WSL/cgroup configuration.

Development may therefore involve a root-capable shell or running the Supervisor with sudo.

This is not a production architecture.

Production direction

The intended future structure is:

API Server
    |
    | unprivileged
    v
Sandbox Supervisor
    |
    | narrowly privileged
    v
Sandbox

The API should never need host-root privileges simply because sandbox setup requires privileged operations.

The future Supervisor should expose a narrow IPC contract over something such as a Unix domain socket.

Potential operations:

CreateExecution
RunExecution
GetExecutionStatus
TerminateExecution

It should not expose arbitrary privileged primitives.

25. Capability Hardening

The eventual production Supervisor should use least privilege.

The project should determine which operations require which Linux capabilities rather than assuming that the entire process must remain unrestricted root.

This is deferred beyond M10.

The current goal is to keep the privileged boundary narrow and make future hardening possible without changing the backend/domain architecture.

26. Testing Status

Latest M9/M10 validation in the current WSL environment:

ProcessRunner suite:
    12 tests run
    11 passed
    1 skipped

Repository CTest:
    1/1 suite passed
    0 failed

The skipped test is the privileged signal-termination case.

It is skipped only when the host cannot perform the actual cgroup.kill operation.

The runtime/lifecycle logic and host-aware test gating are implemented.

Full privileged signal-path validation still requires a root-capable host or privileged container.

27. M10 Scope

M10 was the engine-wide testing and hardening milestone.

It covered the engineering concerns around:

Process lifecycle

normal exit

non-zero exit

invalid executable

signal termination

child/descendant cleanup

I/O

empty input

large input

stdout

stderr

simultaneous output

binary output

EOF

partial I/O

Resource behavior

CPU/time limits

wall-time behavior

memory behavior

signal classification

Failure recovery

pipe failure

fork failure

exec failure

polling/read/write failures

timeout cleanup

reaping

cgroup cleanup

FD cleanup

Sandbox

namespace setup

filesystem isolation

cgroup containment

Supervisor failure handling

host capability-aware tests

The engine is now considered a stable subsystem.

28. Important Lessons Already Established

Pipe deadlock

Blocking on one output stream can deadlock the child when another pipe fills.

poll()

Polling provides an event-loop model that allows stdin, stdout, stderr, control messages, and lifecycle checks to progress together.

fork() vs exec()

fork() creates a process.

exec() replaces its process image.

cgroup containment

Killing one PID does not necessarily terminate descendants.

Supervisor placement

A controller should remain outside the containment boundary it controls.

Namespace semantics

unshare() behavior depends on namespace type. In particular, PID namespace creation affects subsequently created children.

Cleanup

Resource cleanup is part of correctness, not an afterthought.

Privilege

Running the Supervisor with elevated privilege is different from executing untrusted submissions as host root.

29. Current Technical Debt

Deferred items include:

full Linux capability minimization

dedicated Unix-domain-socket Supervisor service

stronger cross-crash cgroup ownership/recovery

abandoned execution-resource cleanup after Supervisor/process crashes

full privileged signal-path validation on a capable host

deeper seccomp hardening

network restrictions

more comprehensive resource accounting

output limits

Compiler dependency injection where still appropriate

Compiler tests

SubmissionService tests

concurrent workers

job queue

persistence

backend API

event-driven architecture

real-time updates

observability

benchmarking

production deployment

These are not reasons to reopen the completed M9/M10 architecture unless a real bug requires it.

30. Next Major Milestone: Backend API

The next system-level direction is:

Client
  |
  v
REST API
  |
  v
SubmissionService
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

The backend should reason in domain concepts:

Submission
Execution
Result
Job
Problem

The Linux sandbox should remain behind the execution boundary.

The backend should not know:

pivot_root
mount namespace
cgroup.procs
cgroup.kill
host PID
namespace PID

unless needed for internal diagnostics.

31. Future Event-Driven Architecture

The execution system should eventually emit domain-level lifecycle events:

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

The execution engine should not communicate directly with browsers.

Long-term:

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

A specific broker or streaming technology has not been selected yet.

32. Future Concurrency

Only after the single execution model is stable should the system introduce:

bounded queue
    |
    v
worker pool
    |
    +--> sandbox
    +--> sandbox
    +--> sandbox

Topics to address:

producer/consumer

mutexes

condition variables

bounded queues

backpressure

cancellation

graceful shutdown

worker failure

33. Future Distributed Execution

Eventually:

API
 |
 v
Queue
 |
 +--> Worker A
 +--> Worker B
 +--> Worker C

Future questions:

What happens if a worker dies?

What if a job executes twice?

How is ownership represented?

How are retries handled?

How is idempotency achieved?

What consistency guarantees are needed?

How is worker health detected?

34. Engineering Rules

Do not add abstractions without a reason.

Do not hide resource ownership.

Do not leave children unreaped.

Do not leak file descriptors.

Do not block on one output stream while another can fill.

Do not confuse cgroup kill with cgroup cleanup.

Do not ignore cleanup failures.

Do not assume partial setup cleans itself.

Do not introduce concurrency before sequential correctness.

Do not introduce distributed execution before worker correctness.

Do not put judging semantics into ProcessRunner.

Do not put raw Linux sandbox operations into the future API layer.

Do not run untrusted submissions directly as host root.

Do not optimize without measurements.

Verify Linux behavior experimentally when correctness depends on kernel semantics.

Prefer behavior-oriented tests.

Keep the privileged Supervisor boundary narrow.

Treat infrastructure failures separately from user-program failures.

35. Definition of Done

A meaningful feature is complete when:

[ ] Requirements understood
[ ] Design alternatives considered
[ ] Architecture chosen
[ ] Implementation complete
[ ] Unit tests added where appropriate
[ ] Integration tests added where appropriate
[ ] Failure cases tested
[ ] Edge cases tested
[ ] Security considered
[ ] Concurrency considered
[ ] Performance considered
[ ] Existing tests pass
[ ] Code reviewed
[ ] Technical debt documented
[ ] Git checkpoint created
[ ] Interview explanation prepared

For sandbox work:

[ ] Resource ownership documented
[ ] Resource lifetime documented
[ ] Partial setup rollback considered
[ ] Cleanup idempotency considered
[ ] Supervisor death considered
[ ] Descendant containment verified

36. Long-Term Target

The final system should eventually look conceptually like:

                         Client
                           |
                           v
                      REST API
                           |
                           v
                   SubmissionService
                     /           \
                    v             v
               Database        Job Queue
                                  |
                                  v
                               Workers
                                  |
                                  v
                               Sandbox
                                  |
                                  v
                              TestRunner
                              /        \
                             v          v
                      ProcessRunner    Judge
                             |
                             v
                      ExecutionResult
                             |
                             v
                       SubmissionResult
                             |
                +------------+------------+
                |                         |
                v                         v
          Event Publisher             Database
                |
                v
          Message Broker
                |
          +-----+-----+
          |           |
          v           v
       Clients     Observability

The current M9/M10 work provides the execution foundation underneath this architecture.

The important design boundary is:

Backend/domain layer
        |
        v
Execution interface
        |
        v
Privileged sandbox Supervisor
        |
        v
Linux namespaces + cgroups + filesystem + process lifecycle
        |
        v
Untrusted submission

The backend should never need to understand Linux sandbox internals merely to submit or retrieve a job.