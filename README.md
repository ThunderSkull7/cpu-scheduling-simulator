# CPU Scheduling Simulator

A high-performance, modular C++20 simulator for standard Operating System CPU scheduling algorithms. The simulator provides deterministic execution tracking, comprehensive validation, Gantt chart visualization, performance metric computation, and comparative algorithm analysis.

---

## Current Status: Batch 4 — Preemptive SRTF Implementation

- [x] **Core Process Model**: Immutable workload data structure (`Process`) with 64-bit time representations.
- [x] **Input Validation Engine**: Comprehensive workload verification (`ProcessValidator`) with multi-error reporting.
- [x] **Process Execution State**: Decoupled dynamic tracking (`ProcessState`) preserving original workload parameters.
- [x] **Execution Timeline**: Complete execution history (`ExecutionSegment`) with explicit CPU idle tracking.
- [x] **FCFS Scheduling Engine**: Non-preemptive First Come, First Served scheduler (`FcfsScheduler`) with stable tie-breaking and arithmetic overflow protection.
- [x] **Non-Preemptive SJF Scheduling Engine**: Shortest Job First scheduler (`SjfScheduler`) with dynamic ready queue selection and three-tier deterministic tie-breaking.
- [x] **Preemptive SRTF Scheduling Engine**: Shortest Remaining Time First scheduler (`SrtfScheduler`) with event-driven simulation, preemption on strictly smaller remaining burst, and deterministic tie-breaking.
- [x] **Scheduling Metrics**: Comprehensive calculation of Turnaround Time, Waiting Time, Response Time, CPU Utilization, and system averages.
- [x] **Gantt Chart Foundation**: Text-based ASCII Gantt chart renderer (`GanttChart`) supporting multiple execution segments per process and idle intervals.
- [x] **Unit Testing Suite**: **172 GoogleTest unit tests** covering all model invariants, validation rules, FCFS, SJF dynamic queue edge cases, SRTF preemptions, and metrics.
- [ ] **Round Robin & Priority Scheduling** *(Planned - Batch 5)*
- [ ] **Interactive CLI & Algorithm Comparison** *(Planned - Batch 6)*

---

## Scheduling Algorithms

### 1. First Come, First Served (FCFS)

- **Philosophy**: Non-preemptive, FIFO arrival-based dispatching.
- **Selection**: The process that arrived earliest in the system executes first.
- **Tie-Breaking**: Relative input order is preserved for simultaneous arrivals.

---

### 2. Shortest Job First (SJF, Non-Preemptive)

- **Philosophy**: Selects the process with the shortest CPU burst time among all processes that have already arrived and are waiting in the ready queue.
- **Non-Preemptive**: Once dispatched, a process runs continuously to completion. A shorter process arriving in the future cannot preempt the currently executing process.
- **Dynamic Ready-Queue Selection**:
  The scheduler dynamically filters candidates such that only processes with $\text{Arrival Time} \le \text{Current Time}$ are eligible for selection. It does **not** sort the global workload purely by burst time.
- **Deterministic Tie-Breaking Rules**:
  1. **Primary**: Shorter burst time has priority.
  2. **Secondary**: Earlier arrival time breaks burst-time ties.
  3. **Tertiary**: Original input sequence order breaks simultaneous arrival ties.
- **Complexity**: $O(n \log n)$ time and $O(n)$ space utilizing a min-priority queue.

---

### 3. Shortest Remaining Time First (SRTF, Preemptive)

- **Philosophy**: Preemptive variant of Shortest Job First. At each scheduling decision, the CPU executes the process with the smallest remaining execution time among all arrived, uncompleted processes.
- **Strict Preemption Rule**:
  - The currently executing process is preempted **if and only if** a competing ready process has a **strictly smaller** remaining execution time (`competing.remaining_time < running.remaining_time`).
  - Equal remaining times do **not** trigger preemption; the running process retains the CPU to avoid redundant context switches.
- **Deterministic Tie-Breaking Rules**:
  1. **Primary**: Shorter remaining execution time.
  2. **Secondary**: Earlier arrival time.
  3. **Tertiary**: Original input workload sequence index.
- **Event-Driven Simulation Engine**:
  - Jumps directly between discrete scheduling events (process arrivals and slice completions) rather than iterating through individual time units ($O(1)$ per step).
  - Time complexity: $O(n \log n)$ where $n$ is the number of processes.
  - Space complexity: $O(n)$.
- **Response Time & Waiting Time Integrity**:
  - Response time is strictly recorded from process arrival until its *first* CPU slice allocation ($RT = \text{First Start Time} - \text{Arrival Time}$), remaining unchanged across subsequent preemptions.
  - Waiting time accurately aggregates all periods spent in the ready queue ($WT = TAT - BT$).

---

## Comparison: FCFS vs. SJF vs. SRTF

Consider the following benchmark workload:

| Process | Arrival Time | Burst Time | Priority |
| :--- | :---: | :---: | :---: |
| **P1** | 0 | 7 | 0 |
| **P2** | 2 | 4 | 0 |
| **P3** | 4 | 1 | 0 |
| **P4** | 5 | 4 | 0 |

### FCFS Execution
- **Order**: P1 (0–7) $\rightarrow$ P2 (7–11) $\rightarrow$ P3 (11–12) $\rightarrow$ P4 (12–16)
- **Gantt Chart**:
  ```text
  | P1 | P2 | P3 | P4 |
  0    7    11   12   16
  ```
- **Average Waiting Time**: **4.75**
- **Average Turnaround Time**: **8.75**

### SJF (Non-Preemptive) Execution
- At $t=0$, only P1 is available $\rightarrow$ P1 executes [0, 7].
- At $t=7$, P2 (burst 4), P3 (burst 1), and P4 (burst 4) have all arrived.
  - P3 has the shortest burst (1) $\rightarrow$ P3 executes [7, 8].
  - P2 and P4 both have burst 4; P2 arrived earlier ($t=2$ vs $t=5$) $\rightarrow$ P2 executes [8, 12].
  - P4 executes [12, 16].
- **Order**: P1 (0–7) $\rightarrow$ P3 (7–8) $\rightarrow$ P2 (8–12) $\rightarrow$ P4 (12–16)
- **Gantt Chart**:
  ```text
  | P1 | P3 | P2 | P4 |
  0    7    8    12   16
  ```
- **Average Waiting Time**: **4.00** *(15.8% reduction vs FCFS)*
- **Average Turnaround Time**: **8.00** *(8.6% reduction vs FCFS)*

### SRTF (Preemptive) Execution
- At $t=0$, P1 begins execution.
- At $t=2$, P1 has 5 units remaining. P2 arrives with burst 4. Because $4 < 5$, P2 **preempts** P1!
- At $t=4$, P2 has 2 units remaining. P3 arrives with burst 1. Because $1 < 2$, P3 **preempts** P2!
- At $t=5$, P3 finishes. P4 arrives with burst 4. Ready queue has P2 (rem 2), P4 (rem 4), and P1 (rem 5). P2 has smallest remaining time $\rightarrow$ P2 resumes and finishes at 7.
- At $t=7$, P4 (rem 4) has shorter remaining time than P1 (rem 5) $\rightarrow$ P4 runs to completion at 11.
- At $t=11$, P1 resumes with its remaining 5 units $\rightarrow$ P1 completes at 16.
- **Order**: P1 (0–2) $\rightarrow$ P2 (2–4) $\rightarrow$ P3 (4–5) $\rightarrow$ P2 (5–7) $\rightarrow$ P4 (7–11) $\rightarrow$ P1 (11–16)
- **Gantt Chart**:
  ```text
  | P1 | P2 | P3 | P2 | P4 | P1 |
  0    2    4    5    7    11   16
  ```
- **Average Waiting Time**: **3.00** *(36.8% reduction vs FCFS!)*
- **Average Turnaround Time**: **7.00** *(20.0% reduction vs FCFS!)*

---

## Scheduling Metrics Definitions

For each process $i$:

| Metric | Formula | Description |
| :--- | :--- | :--- |
| **Completion Time ($CT$)** | Timestamp at process termination | Simulation clock time when remaining execution reaches zero. |
| **Turnaround Time ($TAT$)** | $Completion\ Time - Arrival\ Time$ | Total elapsed duration from process arrival to termination. |
| **Waiting Time ($WT$)** | $Turnaround\ Time - Burst\ Time$ | Total cumulative duration the process waited in the ready queue. |
| **Response Time ($RT$)** | $First\ Start\ Time - Arrival\ Time$ | Duration from arrival until initial CPU allocation. |

### Aggregate Performance Metrics

- **Average Turnaround Time**: $\text{Avg TAT} = \frac{1}{N} \sum_{i=1}^N TAT_i$
- **Average Waiting Time**: $\text{Avg WT} = \frac{1}{N} \sum_{i=1}^N WT_i$
- **Average Response Time**: $\text{Avg RT} = \frac{1}{N} \sum_{i=1}^N RT_i$
- **CPU Utilization**: $\left(\frac{\text{CPU Busy Time}}{\text{Total Simulation Time}}\right) \times 100\%$

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
# Debug test suite (172 tests)
ctest --preset debug-test

# Release test suite (172 tests)
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
│       ├── sjf_scheduler.hpp    # Non-preemptive SJF scheduling engine
│       ├── srtf_scheduler.hpp   # Preemptive SRTF scheduling engine
│       └── gantt_chart.hpp      # Text-based Gantt chart renderer
│
├── src/
│   ├── process.cpp
│   ├── process_validator.cpp
│   ├── process_state.cpp
│   ├── execution_segment.cpp
│   ├── scheduling_result.cpp
│   ├── fcfs_scheduler.cpp
│   ├── sjf_scheduler.cpp
│   ├── srtf_scheduler.cpp
│   └── gantt_chart.cpp
│
├── tests/
│   ├── CMakeLists.txt          # GoogleTest suite target
│   ├── test_process.cpp        # Tests for process data model (7 tests)
│   ├── test_process_validator.cpp # Tests for validation logic (16 tests)
│   ├── test_fcfs_scheduler.cpp # Tests for FCFS scheduling engine (34 tests)
│   ├── test_sjf_scheduler.cpp  # Tests for SJF scheduling engine (52 tests)
│   └── test_srtf_scheduler.cpp # Tests for SRTF scheduling engine (63 tests)
│
├── docs/
│   └── ARCHITECTURE.md         # Component boundaries and lifecycle documentation
│
└── examples/
    ├── CMakeLists.txt          # Example runner target
    └── basic_usage.cpp         # Demonstrates FCFS vs SJF vs SRTF comparison and Gantt charts
```
