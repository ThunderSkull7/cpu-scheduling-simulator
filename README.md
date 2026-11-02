# CPU Scheduling Simulator

A high-performance, modular C++20 simulator for standard Operating System CPU scheduling algorithms. The simulator provides deterministic execution tracking, comprehensive validation, Gantt chart visualization, performance metric computation, and comparative algorithm analysis.

---

## Current Status: Batch 2 — FCFS Scheduling Implementation

- [x] **Core Process Model**: Immutable workload data structure (`Process`) with 64-bit time representations.
- [x] **Input Validation Engine**: Comprehensive workload verification (`ProcessValidator`) with multi-error reporting.
- [x] **Process Execution State**: Decoupled dynamic tracking (`ProcessState`) preserving original workload parameters.
- [x] **Execution Timeline**: Complete execution history (`ExecutionSegment`) with explicit CPU idle tracking.
- [x] **FCFS Scheduling Engine**: Non-preemptive First Come, First Served scheduler (`FcfsScheduler`) with stable tie-breaking and arithmetic overflow protection.
- [x] **Scheduling Metrics**: Comprehensive calculation of Turnaround Time, Waiting Time, Response Time, CPU Utilization, and system averages.
- [x] **Gantt Chart Foundation**: Text-based ASCII Gantt chart renderer (`GanttChart`).
- [x] **Unit Testing Suite**: 57 GoogleTest unit tests covering model invariants, validation rules, FCFS edge cases, and metrics.
- [ ] **Shortest Job First (SJF) & SRTF** *(Planned - Batch 3)*
- [ ] **Round Robin & Priority Scheduling** *(Planned - Batch 4)*
- [ ] **Interactive CLI & Algorithm Comparison** *(Planned - Batch 5)*

---

## First Come, First Served (FCFS) Scheduling

### Algorithm Overview

**First Come, First Served (FCFS)** is a fundamental non-preemptive scheduling algorithm. Processes are dispatched strictly in the order they arrive in the ready queue. Once a process is allocated the CPU, it runs uninterrupted until its entire burst time is consumed.

### Scheduling Rules

1. **Arrival-Order Selection**: Processes are executed in ascending order of their arrival time.
2. **Stable Tie-Breaking**: When multiple processes arrive simultaneously, their relative input order is strictly preserved.
3. **Non-Preemptive Execution**: A running process retains the CPU until completion.
4. **Explicit CPU Idle Periods**: If no process is ready at the current simulation timestamp, the CPU remains idle until the next arrival. Explicit idle segments are inserted into the execution timeline. Zero-length idle intervals are prohibited.
5. **Overflow Protection**: All time calculations and metric accumulations use overflow-checked 64-bit arithmetic (`safe_add`).

---

## Scheduling Metrics Definitions

For each process $i$:

| Metric | Formula | Description |
| :--- | :--- | :--- |
| **Completion Time ($CT$)** | $Start\ Time + Burst\ Time$ | Timestamp at which the process finishes execution. |
| **Turnaround Time ($TAT$)** | $Completion\ Time - Arrival\ Time$ | Total elapsed time from arrival to termination. |
| **Waiting Time ($WT$)** | $Turnaround\ Time - Burst\ Time$ | Total time the process spent waiting in the ready queue. |
| **Response Time ($RT$)** | $First\ Start\ Time - Arrival\ Time$ | Time elapsed from arrival until the process first executes. *(Note: For non-preemptive FCFS, $RT = WT$)* |

### System-Wide Performance Metrics

- **Average Turnaround Time**:
  $$\text{Avg TAT} = \frac{1}{N} \sum_{i=1}^N TAT_i$$
- **Average Waiting Time**:
  $$\text{Avg WT} = \frac{1}{N} \sum_{i=1}^N WT_i$$
- **Average Response Time**:
  $$\text{Avg RT} = \frac{1}{N} \sum_{i=1}^N RT_i$$
- **CPU Utilization**:
  $$\text{CPU Utilization} = \left(\frac{\text{CPU Busy Time}}{\text{Total Simulation Time}}\right) \times 100\%$$

---

## Execution Example

### Input Workload

| Process | Arrival Time | Burst Time | Priority |
| :--- | :---: | :---: | :---: |
| **P1** | 0 | 5 | 2 |
| **P2** | 2 | 3 | 1 |
| **P3** | 4 | 2 | 3 |
| **P4** | 12 | 4 | 1 |

### Execution Trace & Timeline

1. At time `t = 0`, **P1** arrives and executes until `t = 5`.
2. At `t = 5`, **P2** (arrived at `t = 2`) starts and executes until `t = 8`.
3. At `t = 8`, **P3** (arrived at `t = 4`) starts and executes until `t = 10`.
4. At `t = 10`, no process is ready. CPU remains **Idle** from `t = 10` to `t = 12`.
5. At `t = 12`, **P4** arrives and executes until `t = 16`.

### ASCII Gantt Chart

```text
| P1 | P2 | P3 | Idle | P4 |
0    5    8    10     12   16
```

### Metrics Summary

- **P1**: CT = 5, TAT = 5, WT = 0, RT = 0
- **P2**: CT = 8, TAT = 6, WT = 3, RT = 3
- **P3**: CT = 10, TAT = 6, WT = 4, RT = 4
- **P4**: CT = 16, TAT = 4, WT = 0, RT = 0
- **Total Simulation Time**: 16
- **CPU Busy Time**: 14 | **CPU Idle Time**: 2
- **CPU Utilization**: 87.50%
- **Average Turnaround Time**: 5.25
- **Average Waiting Time**: 1.75
- **Average Response Time**: 1.75

---

## Technology Stack

- **Standard**: C++20 (`/std:c++20`, `-std=c++20`)
- **Compilers**: MSVC (Visual Studio 2022 v17.x) / Clang / GCC
- **Build System**: CMake 3.20+ with CMakePresets (Version 3)
- **Package Manager**: vcpkg (Manifest Mode)
- **Testing**: GoogleTest (via vcpkg)
- **Runtime Dependencies**: ISO C++ Standard Library only

---

## Prerequisites

1. **Visual Studio 2022** (v17.4+ recommended) with the "Desktop development with C++" workload installed.
2. **CMake 3.20+** (bundled with VS 2022 or installed on `PATH`).
3. **vcpkg** installed and configured via the `VCPKG_ROOT` environment variable.

---

## Build and Test Instructions

### 1. Environment Setup

Ensure `VCPKG_ROOT` points to your vcpkg installation:

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
```

### 2. Configure Presets

```powershell
# Debug
cmake --preset windows-msvc-debug

# Release
cmake --preset windows-msvc-release
```

### 3. Build Presets

```powershell
# Debug
cmake --build --preset debug

# Release
cmake --build --preset release
```

### 4. Run Unit Tests

```powershell
# Debug test suite (57 tests)
ctest --preset debug-test

# Release test suite (57 tests)
ctest --preset release-test
```

### 5. Run Example Application

```powershell
# Debug
.\build\debug\examples\Debug\basic_usage.exe

# Release
.\build\release\examples\Release\basic_usage.exe
```

---

## Project Structure

```text
CPU Scheduling Simulator/
│
├── CMakeLists.txt              # Top-level CMake project configuration
├── CMakePresets.json           # Multi-configuration presets (MSVC x64 Debug/Release)
├── vcpkg.json                  # vcpkg manifest defining dependencies (GoogleTest)
├── .gitignore                  # Git ignore rules for build artifacts and IDE files
├── README.md                   # Project overview, algorithm guide, and instructions
│
├── include/
│   └── cpu_scheduler/
│       ├── process.hpp          # Immutable process workload model
│       ├── process_validator.hpp# Workload validation interface
│       ├── process_state.hpp    # Dynamic execution tracking model
│       ├── execution_segment.hpp# Timeline segment (Process / Idle)
│       ├── scheduling_result.hpp# Results container & error handling
│       ├── fcfs_scheduler.hpp   # FCFS scheduling engine
│       └── gantt_chart.hpp      # Text-based Gantt chart renderer
│
├── src/
│   ├── process.cpp
│   ├── process_validator.cpp
│   ├── process_state.cpp
│   ├── execution_segment.cpp
│   ├── scheduling_result.cpp
│   ├── fcfs_scheduler.cpp
│   └── gantt_chart.cpp
│
├── tests/
│   ├── CMakeLists.txt          # GoogleTest suite target
│   ├── test_process.cpp        # Tests for process data model
│   ├── test_process_validator.cpp # Tests for validation logic
│   └── test_fcfs_scheduler.cpp # Comprehensive FCFS and metric tests (34 tests)
│
├── docs/
│   └── ARCHITECTURE.md         # Component boundaries and lifecycle documentation
│
└── examples/
    ├── CMakeLists.txt          # Example runner target
    └── basic_usage.cpp         # Demonstrates FCFS execution and Gantt chart output
```
