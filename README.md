# Online Judge

A production-oriented Online Judge built from scratch in Modern C++ on Linux.

The project focuses on understanding how real coding platforms compile, execute, isolate, evaluate, and eventually scale the execution of untrusted user programs.

The goal is not simply to build a Codeforces-style judge, but to use the system as a vehicle for learning Linux systems programming, operating systems, backend engineering, security, concurrency, and distributed systems.

---

## Objectives

This project is designed to gain hands-on experience with:

- Linux Systems Programming
- Operating Systems
- Process Management
- Inter-Process Communication (IPC)
- Software Architecture
- Backend Engineering
- Compiler Integration
- Resource Management
- Sandboxing
- Docker & Containerization
- Databases
- REST APIs
- Concurrency
- Distributed Systems
- Performance Benchmarking

---

# Current Architecture

The current system supports the complete basic submission pipeline:

```text
                       SubmissionService
                              │
                              ▼
                          Compiler
                              │
                              ▼
                        ProcessRunner
                              │
                              ▼
                        CompileResult
                              │
                    ┌─────────┴─────────┐
                    │                   │
                 Failed              Success
                    │                   │
                    ▼                   ▼
            Compilation Error     ProcessRunner
                                        │
                                        ▼
                                ExecutionResult
                                        │
                                        ▼
                                      Judge
                                        │
                                        ▼
                                  JudgeResult
                                        │
                                        ▼
                                SubmissionResult
```

The architecture separates:

- **Process execution** — `ProcessRunner`
- **Compilation** — `Compiler`
- **Evaluation** — `Judge`
- **Submission orchestration** — `SubmissionService`
- **Result representation** — dedicated result structures

This allows the execution engine to be reused by multiple subsystems.

---

# Features Implemented

## Process Execution Engine

Implemented a reusable Linux process execution layer capable of:

- Creating processes using `fork()`
- Replacing process images using `execvp()`
- Waiting for child processes using `waitpid()`
- Passing arbitrary command-line arguments
- Capturing standard output using Unix pipes
- Capturing standard error using a separate Unix pipe
- Redirecting file descriptors using `dup2()`
- Detecting normal process termination through exit status

### Process execution flow

```text
Parent
  │
  ├── create stdout pipe
  ├── create stderr pipe
  ├── fork()
  │
  ├────────────── Child
  │                  │
  │                  ├── dup2(stdout pipe, STDOUT)
  │                  ├── dup2(stderr pipe, STDERR)
  │                  └── execvp()
  │
  └── Parent
       │
       ├── waitpid()
       ├── read stdout
       └── read stderr
```

---

## Compiler

Implemented a compiler abstraction on top of `ProcessRunner`.

The compiler currently invokes:

```text
g++ source.cpp -o executable
```

and produces a `CompileResult` containing:

- Compiler exit code
- Compiler diagnostics from `stderr`
- Executable path when compilation succeeds

Compilation failures stop the submission pipeline before execution.

---

## Judge

Current judging supports:

- Accepted
- Wrong Answer
- Runtime Error

The Judge operates independently from process execution.

It receives execution facts and determines the result of comparing the program's behavior with the expected output.

---

## Submission Service

`SubmissionService` orchestrates the complete evaluation lifecycle:

```text
Source Code
    │
    ▼
Compiler
    │
    ▼
CompileResult
    │
    ├── Compilation Failure
    │       └── Stop
    │
    └── Compilation Success
            │
            ▼
       ProcessRunner
            │
            ▼
      ExecutionResult
            │
            ▼
          Judge
            │
            ▼
       JudgeResult
            │
            ▼
      SubmissionResult
```

`std::optional` is used to represent execution and judging stages that were never reached because an earlier stage failed.

For example:

```text
Compilation Failure

CompileResult       → present
ExecutionResult     → absent
JudgeResult         → absent
Final Verdict       → Compilation Error
```

---

# Result Model

The system separates information produced by different layers.

```text
CompileResult
    │
    ├── exit code
    ├── compiler stderr
    └── executable path

ExecutionResult
    │
    ├── exit code
    ├── stdout
    ├── stderr
    ├── execution time
    ├── memory usage
    └── termination information

JudgeResult
    │
    └── evaluation of program behavior

SubmissionResult
    │
    ├── CompileResult
    ├── optional ExecutionResult
    ├── optional JudgeResult
    └── final Verdict
```

This separation prevents lower-level components from making higher-level decisions.

---

# Linux Concepts Used

The project currently uses:

- `fork()`
- `execvp()`
- `waitpid()`
- `pipe()`
- `dup2()`
- File Descriptors
- Parent/Child Processes
- Process Exit Status
- Standard Input/Output/Error
- Inter-Process Communication
- Copy-on-Write
- Process Image Replacement

---

# Current Project Structure

```text
include/
├── common/
├── compiler/
├── judge/
├── runner/
└── submission/

src/
├── compiler/
├── judge/
├── runner/
└── submission/

tests/
└── ...
```

---

# Roadmap

## Core Execution Engine

- [x] Project setup
- [x] CMake build system
- [x] Process execution
- [x] Command-line argument support
- [x] Standard output capture
- [x] Standard error capture
- [x] Basic Judge
- [x] Submission Service
- [x] Compiler integration

---

## Execution Platform

- [ ] Standard input redirection
- [ ] Multiple test case execution
- [ ] Robust streaming output capture
- [ ] Execution time measurement
- [ ] Memory measurement
- [ ] Time Limit Exceeded (TLE)
- [ ] Memory Limit Exceeded (MLE)
- [ ] Runtime signal handling
- [ ] Large-output handling
- [ ] Process cleanup and failure recovery

---

## Security & Sandboxing

- [ ] Process isolation
- [ ] Linux resource limits
- [ ] Restricted filesystem access
- [ ] Restricted system calls
- [ ] Docker sandbox
- [ ] Container resource limits
- [ ] Secure execution environment

---

## Backend

- [ ] Submission API
- [ ] REST API
- [ ] Authentication
- [ ] Submission persistence
- [ ] Database integration
- [ ] Result persistence
- [ ] Problem management
- [ ] Submission history

---

## Concurrency & Distributed Systems

- [ ] Submission queue
- [ ] Worker pool
- [ ] Concurrent execution
- [ ] Bounded job queue
- [ ] Worker failure handling
- [ ] Scheduler
- [ ] Backpressure
- [ ] Distributed execution
- [ ] Horizontal scaling

---

## Observability & Performance

- [ ] Structured logging
- [ ] Execution metrics
- [ ] Submission latency measurement
- [ ] Throughput measurement
- [ ] P50/P95/P99 latency
- [ ] CPU utilization measurement
- [ ] Memory utilization measurement
- [ ] Benchmark suite
- [ ] Sequential vs concurrent execution benchmarks

Performance improvements will be measured using reproducible workloads rather than estimated or assumed numbers.

---

## Deployment

- [ ] Dockerized services
- [ ] Production configuration
- [ ] CI/CD
- [ ] Deployment
- [ ] Monitoring
- [ ] Health checks

---

# Technologies

- C++20
- CMake
- Linux
- POSIX APIs
- Git
- Docker
- REST
- SQL
- Linux Containers

---

# Engineering Principles

The project emphasizes:

### Separation of Concerns

Process execution, compilation, judging, and orchestration are separate responsibilities.

### Reusability

`ProcessRunner` is designed as reusable infrastructure rather than being tightly coupled to the compiler or judge.

### Explicit Result Modeling

Different stages return structured results instead of hiding information inside higher-level abstractions.

### Failure-Aware Design

The system explicitly models stages that were not executed using `std::optional`.

### Production Thinking

Features are designed with:

- Correctness
- Security
- Performance
- Scalability
- Reliability
- Maintainability

in mind.

---

# Learning Outcomes

This project is being developed to build practical understanding of:

- Linux Internals
- Operating Systems
- Process Lifecycle
- System Calls
- Inter-Process Communication
- Compiler Toolchains
- Backend Architecture
- Resource Management
- Security and Sandboxing
- Concurrency
- Distributed Systems
- Performance Engineering
- Production System Design

