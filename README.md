# Online Judge

A production-oriented Online Judge built from scratch in Modern C++ on Linux.

The project focuses on understanding how real coding platforms execute, isolate, evaluate, and manage untrusted user programs.

---

## Objectives

This project is built to gain hands-on experience with:

- Linux Systems Programming
- Operating Systems
- Process Management
- Inter-Process Communication (IPC)
- Backend Engineering
- Software Architecture
- Docker & Containerization
- Resource Isolation
- Databases
- REST APIs
- Distributed Systems

The objective is not merely to build an Online Judge, but to understand the engineering principles used in production-grade execution platforms.

---

# Current Architecture

```
                SubmissionService
                        │
                        ▼
                 ProcessRunner
                        │
                fork() / execvp()
                        │
                ExecutionResult
                        │
                        ▼
                     Judge
                        │
                        ▼
                  JudgeResult
```

The architecture separates execution from evaluation, allowing reusable infrastructure for future modules such as the compiler and custom judges.

---

# Features Implemented

## Process Execution Engine

- Execute external programs using `fork()`
- Replace process image using `execvp()`
- Parent-child synchronization using `waitpid()`
- Output capture through Unix Pipes
- File descriptor redirection using `dup2()`

---

## Judge

Current judging supports:

- Accepted
- Wrong Answer
- Runtime Error

The Judge operates independently from process execution and evaluates only the execution results.

---

## Submission Pipeline

Implemented a service layer that orchestrates:

```
Submission
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
SubmissionResult
```

This layered architecture mirrors production backend systems where orchestration is separated from infrastructure.

---

# Linux Concepts Used

- fork()
- execvp()
- waitpid()
- pipe()
- dup2()
- File Descriptors
- Parent/Child Processes
- Inter-Process Communication (IPC)

---

# Current Project Structure

```
include/
├── common/
├── judge/
├── runner/
└── submission/

src/
├── common/
├── judge/
├── runner/
└── submission/
```

---

# Roadmap

## Core Engine

- [x] Project setup
- [x] CMake build system
- [x] Process execution engine
- [x] Output capture
- [x] Basic Judge
- [x] Submission Service

---

## Execution Platform

- [ ] Compiler module
- [ ] Standard input redirection
- [ ] Standard error capture
- [ ] Multiple test case execution
- [ ] Time measurement
- [ ] Memory measurement
- [ ] Time Limit Exceeded (TLE)
- [ ] Memory Limit Exceeded (MLE)
- [ ] Runtime Signal Handling

---

## Security

- [ ] Process isolation
- [ ] Linux resource limits
- [ ] Docker sandbox
- [ ] Restricted execution environment

---

## Backend

- [ ] REST API
- [ ] Submission queue
- [ ] Database integration
- [ ] Result persistence
- [ ] Authentication
- [ ] Deployment

---

# Technologies

- Modern C++20
- CMake
- Linux
- POSIX System Calls
- Git

---

# Learning Outcomes

This project is designed to develop practical understanding of:

- Linux Internals
- Operating Systems
- Process Lifecycle
- System Calls
- Software Architecture
- Backend Engineering
- Production System Design

