# CPU Scheduling Simulator

A high-performance, modular C++20 simulator for standard Operating System CPU scheduling algorithms. The simulator provides deterministic execution tracking, comprehensive validation, Gantt chart visualization, performance metric computation, and comparative algorithm analysis.

---

## Current Status: Batch 12 — CSV Workload Import and Results Export

- [x] **Core Process Model**: Immutable workload data structure (`Process`) with 64-bit time representations and priority integers.
- [x] **Input Validation Engine**: Comprehensive workload verification (`ProcessValidator`) with multi-error reporting.
- [x] **Process Execution State**: Decoupled dynamic tracking (`ProcessState`) preserving original workload parameters and assigned `queue_id`.
- [x] **Execution Timeline**: Complete execution history (`ExecutionSegment`) with explicit CPU idle tracking.
- [x] **FCFS Scheduling Engine**: Non-preemptive First Come, First Served scheduler (`FcfsScheduler`) with stable tie-breaking and arithmetic overflow protection.
- [x] **Non-Preemptive SJF Scheduling Engine**: Shortest Job First scheduler (`SjfScheduler`) with dynamic ready queue selection and three-tier deterministic tie-breaking.
- [x] **Preemptive SRTF Scheduling Engine**: Shortest Remaining Time First scheduler (`SrtfScheduler`) with event-driven simulation, preemption on strictly smaller remaining burst, and deterministic tie-breaking.
- [x] **Preemptive Round Robin (RR) Scheduling Engine**: Round Robin scheduler (`RrScheduler`) with configurable time quantum, strict FIFO queue rotation, simultaneous arrival handling, timeline normalization, and overflow protection.
- [x] **Non-Preemptive Priority Scheduling Engine**: Priority scheduler (`NonPreemptivePriorityScheduler`) selecting highest-priority ready processes (smaller numerical value = higher priority) with stable tie-breaking.
- [x] **Preemptive Priority Scheduling Engine**: Priority scheduler (`PreemptivePriorityScheduler`) featuring event-driven simulation, preemption on strictly higher priority (`new_proc.priority < running_proc.priority`), equal-priority non-preemption, and preserved remaining burst times.
- [x] **Multilevel Queue (MLQ) Scheduling Engine**: Comprehensive multilevel queue scheduler (`MlqScheduler`) featuring multiple independent ready queues, permanent process-to-queue binding, configurable queue algorithms (FCFS, SJF, SRTF, RR, Non-Preemptive Priority, Preemptive Priority), and configurable inter-queue policies (Fixed-Priority Preemptive and Round Robin between queues).
- [x] **Multilevel Feedback Queue (MLFQ) Scheduling Engine**: Dynamic priority feedback scheduler (`MlfqScheduler`) featuring multiple priority levels, initial Q0 admission, dynamic demotion via per-level CPU allotments, preemption by higher-priority arrivals without allotment reset, configurable periodic priority boosting to prevent starvation, and timeline normalization.
- [x] **Unified Scheduling Engine & Central Dispatcher**: Type-safe central execution dispatcher (`SchedulingEngine` / `UnifiedScheduler`) supporting uniform execution of all 8 algorithms through a consistent `SchedulingRequest` interface, configuration validation, introspection, and result compatibility.
- [x] **Interactive CLI Application**: Decoupled, stream-based interactive command-line interface (`CliController`) supporting interactive menu navigation, field-level workload input and validation, algorithm configurations (RR, MLQ, MLFQ), simulation dispatch through `SchedulingEngine`, aligned tabular results, metrics reporting, and multi-simulation sessions.
- [x] **Advanced Gantt Chart Renderer**: Duration-aware text Gantt chart generator (`GanttChart`) with proportional block scaling, boundary timestamp alignment under segment borders, multi-row wrapping at configurable character widths, explicit idle tracking, Unicode box-drawing support, and MLQ queue metadata visualization.
- [x] **Dedicated Results Presentation Engine**: Modular formatting engine (`ResultsRenderer`) delivering structured Simulation Summaries, aligned tabular process execution details, aggregate metrics, chronological timelines, and integrated full reports.
- [x] **RFC 4180 CSV Engine**: Built-in compliant CSV parser (`CsvParser`) supporting quoted fields, escaped quotes (`""`), embedded commas, CRLF/LF line endings, and syntax error detection.
- [x] **Workload CSV Importer**: Type-safe workload ingestion engine (`WorkloadImporter`) supporting required (`process_id`, `arrival_time`, `burst_time`) and optional (`priority`, `queue_id`) columns, case-insensitive headers, trimmed whitespace, all-or-nothing validation, duplicate ID rejection, and row diagnostics.
- [x] **Results CSV Exporter**: Robust export engine (`CsvExporter`) producing spreadsheet-compatible process execution metrics and chronological execution timeline CSVs with formula injection mitigation (`=`, `+`, `-`, `@`) and safe staged file replacement (via `ReplaceFileW` on Windows).
- [x] **Scheduling Metrics**: Comprehensive calculation of Turnaround Time, Waiting Time, Response Time, CPU Utilization, and system averages.
- [x] **Unit Testing Suite**: **571 GoogleTest unit tests** covering all model invariants, validation rules, all 8 scheduling engines, unified scheduling engine integration, interactive CLI workflows, Gantt chart layouts, results formatting, CSV parser, workload importer, results exporter, security injection defense, and CLI CSV workflows.*

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

### 4. Round Robin (RR, Preemptive)

- **Philosophy**: Fixed-time-slice preemptive scheduling designed for fair time-sharing. Every ready process is allocated a maximum continuous execution time equal to the **time quantum** ($q$).
- **FIFO Ready Queue & Rotation**:
  - Processes are queued in a strict FIFO ready queue.
  - The process at the head runs for $\min(q, \text{remaining\_burst})$.
- **Quantum Expiration & Boundary Arrivals**:
  - When a quantum expires, all processes arriving at or before the expiration timestamp are admitted into the ready queue in stable arrival/input order **before** the preempted process is appended to the tail of the queue.
  - If a process finishes its execution within or at its quantum, it terminates and is never requeued.
- **No Unnecessary Context Switches (Normalization)**:
  - If a process finishes its quantum and no other process is waiting in the ready queue, it retains the CPU.
  - Consecutive execution slices for the same process are normalized into a single continuous timeline segment.
- **Complexity**: $O(n \log n + \frac{\text{Total Burst}}{q})$ time and $O(n + \frac{\text{Total Burst}}{q})$ timeline slice space.

---

### 5. Non-Preemptive Priority Scheduling

- **Philosophy**: Allocates the CPU to the ready process with the highest priority. Once dispatched, the process runs to completion uninterrupted.
- **Priority Direction Convention**:
  - **Smaller numerical value represents higher scheduling priority**.
  - Example: Priority 1 has higher priority than Priority 2; Priority -5 has higher priority than Priority 0.
  - Safe relational comparisons (`>`) are performed directly on integer values without subtraction, eliminating signed overflow risk.
- **Deterministic Tie-Breaking Rules**:
  1. **Primary**: Highest priority (smallest numerical value).
  2. **Secondary**: Earlier arrival time.
  3. **Tertiary**: Original input workload sequence index.
- **Complexity**: $O(n \log n)$ time and $O(n)$ space utilizing a min-priority queue.

---

### 6. Preemptive Priority Scheduling

- **Philosophy**: Preemptive variant of Priority Scheduling. The CPU is allocated to the highest-priority ready process at all times.
- **Strict Preemption Rule**:
  - A newly arriving process preempts the currently running process **if and only if** it has a **strictly higher priority**:
    $$\text{new\_process.priority} < \text{running\_process.priority}$$
  - **Equal-Priority Arrivals**: Do **not** preempt the running process. The running process retains the CPU.
  - **Lower-Priority Arrivals**: Do **not** preempt the running process.
- **Preservation of Remaining Time**:
  - When preempted, the active process's remaining burst time is preserved and it returns to the ready pool.
- **Deterministic Tie-Breaking (Ready Pool)**:
  1. Highest priority (smallest numerical value).
  2. Earlier arrival time.
  3. Original input index.
- **Event-Driven Simulation Engine**:
  - Jumps directly between discrete scheduling events: next arrival or active process completion.
  - Bounded by at most $n$ arrivals and at most $n$ preemptions, guaranteeing an exact time complexity of $O(n \log n)$ and auxiliary space of $O(n)$ (at most $2n - 1$ timeline segments).
- **Starvation Limitation & Aging**:
  - Fixed-priority scheduling can lead to indefinite starvation of lower-priority processes if higher-priority work continuously arrives.
  - Priority aging (gradually elevating the priority of long-waiting processes) will be implemented in a subsequent batch.

---

### 7. Multilevel Queue (MLQ) Scheduling

- **Philosophy**: Partitions processes permanently into distinct, independent ready queues based on process category (e.g., system services, interactive tasks, batch jobs).
- **MLQ vs. MLFQ Distinction**:
  - In **MLQ**, a process is permanently assigned to exactly one queue for the entire simulation. Processes do **not** move between queues, and feedback-based queue reassignment or priority aging is excluded.
- **Queue Priority vs. Process Priority**:
  - **Queue Priority**: Determines which ready queue is selected to execute by the inter-queue scheduler. Smaller numerical queue priority indicates higher scheduling preference ($0 < 1 < 2$).
  - **Process Priority**: Governs intra-queue candidate ordering when a queue uses a priority-based internal algorithm (`NonPreemptivePriority` or `PreemptivePriority`).
- **Configurable Inter-Queue Scheduling Policies**:
  1. `InterQueuePolicy::FixedPriorityPreemptive`:
     - The highest-priority non-empty queue always executes.
     - An arriving process in a strictly higher-priority queue preempts any currently executing process in a lower-priority queue immediately.
     - Deterministic tie-breaking for equal queue priorities favors the queue declared earlier in configuration order.
  2. `InterQueuePolicy::RoundRobin`:
     - CPU time is distributed cyclically across all non-empty ready queues using an inter-queue time quantum ($Q_{\text{inter}}$).
     - Empty queues are skipped instantaneously without CPU time consumption.
     - An interrupted queue resumes execution in its next cycle, retaining process progress and state.
- **Supported Intra-Queue Algorithms**:
  - `QueueAlgorithm::Fcfs` (First-Come, First-Served)
  - `QueueAlgorithm::Sjf` (Non-Preemptive Shortest Job First)
  - `QueueAlgorithm::Srtf` (Preemptive Shortest Remaining Time First)
  - `QueueAlgorithm::RoundRobin` (Intra-queue Round Robin with quantum $q_{\text{internal}}$)
  - `QueueAlgorithm::NonPreemptivePriority` (Non-preemptive priority within queue)
  - `QueueAlgorithm::PreemptivePriority` (Preemptive priority within queue)
- **Two-Level Quantum Interaction**:
  - When a queue uses internal Round Robin ($q$) and the inter-queue policy is Round Robin ($Q$), both quanta decrease concurrently by the elapsed slice duration $\Delta t$. If the internal quantum expires, the process is rotated within the queue; if the inter-queue quantum expires, the CPU switches to the next non-empty queue while preserving the active process's remaining internal slice.
- **Complexity**:
  - Let $K$ be the number of queues, $N$ be total processes, and $B$ be total burst time.
  - Fixed-Priority Inter-Queue: $O(N \log N + K \cdot E)$ where $E$ is the number of scheduling events ($\le 2N$).
  - Round Robin Inter-Queue: $O(N \log N + \frac{B}{Q_{\text{inter}}} \cdot K)$ time and $O(N + \frac{B}{Q_{\text{inter}}})$ space.

---

### 8. Multilevel Feedback Queue (MLFQ) Scheduling

- **Philosophy**: Dynamic priority adjustment based on CPU burst history. Interactive/short jobs receive high priority and fast response, while compute-intensive long jobs gradually sink to lower priority levels with larger time slices.
- **MLQ vs. MLFQ Distinction**:
  - In **MLQ**, a process is permanently fixed in its designated queue.
  - In **MLFQ**, all processes arrive at the top queue (Queue 0) and dynamically migrate down (demoted upon exhausting level CPU allotment) or up (promoted upon periodic priority boost).
- **Core MLFQ Rules & Invariants**:
  1. **Rule 1 (Priority Hierarchy)**: If $\text{Priority}(A) < \text{Priority}(B)$ (numerically smaller level index), Process $A$ runs and $B$ does not.
  2. **Rule 2 (Round Robin within Level)**: If $\text{Priority}(A) == \text{Priority}(B)$, processes share CPU in FIFO Round Robin order using the level's `time_quantum`.
  3. **Rule 3 (Top Entry)**: When a process enters the system, it is placed at Queue 0.
  4. **Rule 4 (Dynamic Demotion)**: Once a process uses up its time allotment at a given level (regardless of how many times it yields or gets preempted), its priority is reduced by 1 level ($Q_l \to Q_{l+1}$). Bottom-level processes stay at the bottom level.
  5. **Rule 5 (Periodic Priority Boost)**: After `boost_interval` time units, all uncompleted processes in the system are moved to Queue 0 and their allotments and quanta are reset, preventing starvation.
- **Allotment Tracking across Preemptions**:
  - If a process running at level $l > 0$ is preempted by a new arrival entering Queue 0, the preempted process's accumulated CPU allotment is preserved. It does **not** get a fresh allotment simply by being interrupted.
  - The preempted process is placed at the front of level $l$'s queue (`push_front`) to resume its slice immediately once higher-priority queues are serviced.
- **Deterministic Boost & Tie-Breaking**:
  - At exact boost timestamps, executing processes are interrupted and timeline segments closed.
  - All uncompleted processes are promoted to Queue 0 in deterministic order:
    1. Earlier arrival time.
    2. Original input workload sequence index.
- **Complexity**: $O(N \log N + \frac{\text{Total Burst}}{\min(q)} + K_{\text{boost}} \cdot N \log N)$ time and $O(N + \frac{\text{Total Burst}}{\min(q)})$ space.

---

### 9. Unified Scheduling Engine (`SchedulingEngine` / `UnifiedScheduler`)

- **Philosophy**: Common dispatch and execution facade decoupling callers (CLI, reporting, comparison benchmarks) from concrete scheduler implementations.
- **Unified Algorithm Selection**: `enum class SchedulingAlgorithm` strictly distinguishes all 8 algorithm variants (`Fcfs`, `Sjf`, `Srtf`, `RoundRobin`, `NonPreemptivePriority`, `PreemptivePriority`, `MultilevelQueue`, `MultilevelFeedbackQueue`).
- **Type-Safe Scheduling Request (`SchedulingRequest`)**:
  - Encapsulates target algorithm, workload, and algorithm-specific configuration in `std::variant<std::monostate, RoundRobinConfig, MlqWorkload, MlfqConfig>`.
  - Convenient static factories (`SchedulingRequest::fcfs()`, `round_robin()`, `mlq()`, `mlfq()`, etc.) and fluent builder methods.
- **Strict Dispatch Validation**:
  - Rejects irrelevant configurations passed to non-configurable algorithms.
  - Validates required configuration parameters (e.g. positive Round Robin quantum, valid MLQ workload, valid MLFQ tiers).
  - Preserves underlying scheduler return types (`ScheduleOutcome`, `SchedulingResult`, `ScheduleError`).
- **Zero Overhead & Full Parity**:
  - Dispatches directly to underlying schedulers with zero algorithmic duplication.
  - Returns identical execution timelines, per-process metrics, and aggregate statistics.

---

## Algorithm Comparison Benchmarks

Consider the following benchmark workload:

| Process | Arrival Time | Burst Time | Priority |
| :--- | :---: | :---: | :---: |
| **P1** | 0 | 7 | 3 |
| **P2** | 2 | 4 | 1 |
| **P3** | 4 | 1 | 2 |
| **P4** | 5 | 4 | 1 |

### Execution Breakdown:

1. **FCFS**:
   - Order: P1(0-7) $\rightarrow$ P2(7-11) $\rightarrow$ P3(11-12) $\rightarrow$ P4(12-16)
   - Avg Waiting Time: 4.75 | Avg Turnaround Time: 8.75 | CPU Utilization: 100.0%
2. **SJF (Non-Preemptive)**:
   - Order: P1(0-7) $\rightarrow$ P3(7-8) $\rightarrow$ P2(8-12) $\rightarrow$ P4(12-16)
   - Avg Waiting Time: 4.00 | Avg Turnaround Time: 8.00 | CPU Utilization: 100.0%
3. **SRTF (Preemptive)**:
   - Order: P1(0-2) $\rightarrow$ P2(2-4) $\rightarrow$ P3(4-5) $\rightarrow$ P2(5-7) $\rightarrow$ P4(7-11) $\rightarrow$ P1(11-16)
   - Avg Waiting Time: 3.00 | Avg Turnaround Time: 7.00 | CPU Utilization: 100.0%
4. **Round Robin (q=2)**:
   - Order: P1(0-2) $\rightarrow$ P2(2-4) $\rightarrow$ P1(4-6) $\rightarrow$ P3(6-7) $\rightarrow$ P2(7-9) $\rightarrow$ P4(9-11) $\rightarrow$ P1(11-13) $\rightarrow$ P4(13-15) $\rightarrow$ P1(15-16)
   - Avg Waiting Time: 5.00 | Avg Turnaround Time: 9.00 | CPU Utilization: 100.0%
5. **Non-Preemptive Priority**:
   - Order: P1(0-7) $\rightarrow$ P2(7-11) $\rightarrow$ P4(11-15) $\rightarrow$ P3(15-16)
   - Avg Waiting Time: 5.50 | Avg Turnaround Time: 9.50 | CPU Utilization: 100.0%
6. **Preemptive Priority**:
   - Order: P1(0-2) $\rightarrow$ P2(2-6) $\rightarrow$ P4(6-10) $\rightarrow$ P3(10-11) $\rightarrow$ P1(11-16)
   - Avg Waiting Time: 4.00 | Avg Turnaround Time: 8.00 | CPU Utilization: 100.0%

---

## Core Metrics Definitions

- **Completion Time ($CT$)**: Timestamp at which a process finishes its execution.
- **Turnaround Time ($TAT$)**: Total elapsed time from arrival to completion ($TAT = CT - AT$).
- **Waiting Time ($WT$)**: Total time spent waiting in the ready queue ($WT = TAT - BT$).
- **Response Time ($RT$)**: Duration from arrival until the first time the process receives CPU execution ($RT = ST_{\text{first}} - AT$).
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
# Debug test suite (511 tests)
ctest --preset debug-test

# Release test suite (511 tests)
ctest --preset release-test
```

### 5. Run Example Application

```powershell
# Debug
.\build\debug\examples\Debug\basic_usage.exe

# Release
.\build\release\examples\Release\basic_usage.exe
```

### 6. Launch Interactive CLI Application

```powershell
# Debug CLI
.\build\debug\Debug\cpu_scheduler_cli.exe

# Release CLI
.\build\release\Release\cpu_scheduler_cli.exe
```

---

## Interactive CLI User Guide

The interactive CLI provides a user-friendly, terminal-based simulation environment built directly on top of `cpu_scheduler::SchedulingEngine`.

### Main Menu

Upon launch, the CLI presents the algorithm selection menu:

```text
============================================================
              CPU SCHEDULING SIMULATOR - CLI
============================================================

Available Scheduling Algorithms:
  1. First Come, First Served (FCFS)
  2. Shortest Job First (SJF, Non-Preemptive)
  3. Shortest Remaining Time First (SRTF, Preemptive)
  4. Round Robin (RR)
  5. Priority (Non-Preemptive)
  6. Priority (Preemptive)
  7. Multilevel Queue (MLQ)
  8. Multilevel Feedback Queue (MLFQ)
  9. Exit
============================================================
Enter selection (1-9):
```

### Workload & Configuration Guidelines

1. **Standard Workloads (FCFS, SJF, SRTF, RR, Priority, MLFQ)**:
   - **Process Count**: 1 to 100 processes per simulation.
   - **Process ID**: Non-empty, unique across the workload (e.g. `P1`, `P2`). Duplicate IDs (case-insensitive) are rejected and re-prompted.
   - **Arrival Time**: Integer $\ge 0$.
   - **Burst Time**: Strictly positive integer $> 0$.
   - **Priority**: Integer (smaller numerical value = higher priority). Only prompted when selecting Priority (Non-Preemptive or Preemptive) or MLQ.

2. **Round Robin (RR)**:
   - Prompts for **Time Quantum** ($> 0$).

3. **Multilevel Queue (MLQ)**:
   - **Queue Setup**: Define 1 to 10 queues. For each queue, enter its unique ID (e.g. `Q1`), priority integer, and intra-queue algorithm (FCFS, SJF, SRTF, RR, Priority NP, Priority P). If RR is chosen, an internal quantum is prompted.
   - **Inter-Queue Policy**: Select between `Fixed Priority Preemptive` (higher priority queue strictly preempts lower) and `Round Robin` (cyclic queue execution with default inter-queue quantum).
   - **Process Assignment**: For each process, enter process ID, arrival, burst, priority, and assign to one of the defined queues.

4. **Multilevel Feedback Queue (MLFQ)**:
   - **Level Setup**: 2 to 10 priority levels (Level 0 is highest priority).
   - **Per-Level Quantum & Allotment**: Enter time quantum ($> 0$) and optionally a custom CPU allotment ($\ge$ quantum).
   - **Priority Boost**: Optionally enable periodic priority boosting by specifying a positive boost interval ($> 0$).
   - **Workload**: Processes enter Queue 0 dynamically based on feedback rules.

### Results Presentation

After execution, the CLI formats:
- **Simulation Summary**: Algorithm name, process count, total simulation time, CPU busy/idle times, and CPU utilization percentage.
- **Process Details Table**: Process ID, arrival, burst, priority (when applicable), queue ID (when applicable), completion time, turnaround time, waiting time, and response time.
- **Aggregate Metrics**: System-wide averages for turnaround, waiting, and response times, plus CPU utilization.
- **Execution Timeline**: Chronological trace of scheduled process execution slices and CPU idle intervals.

### Example Workflow: Round Robin Execution

```powershell
.\build\release\Release\cpu_scheduler_cli.exe
```

Input sequence:
```text
4          # Select Round Robin (RR)
2          # Time quantum = 2
2          # 2 processes
P1         # PID
0          # Arrival time
3          # Burst time
P2         # PID
1          # Arrival time
3          # Burst time
2          # Exit post-simulation
```

Output:
```text
============================================================
                    SIMULATION SUMMARY
============================================================
Algorithm:                Round Robin (RR)
Process Count:            2
Total Simulation Time:    6
CPU Busy Time:            6
CPU Idle Time:            0
CPU Utilization:          100.00%
============================================================

Process Execution Details:
PID       Arrival   Burst  Completion  Turnaround   Waiting  Response
---------------------------------------------------------
P1              0       3           5           5         2         0
P2              1       3           6           5         2         1
---------------------------------------------------------

============================================================
                     AGGREGATE METRICS
============================================================
Average Turnaround Time:    5.00
Average Waiting Time:       2.00
Average Response Time:      0.50
CPU Utilization:            100.00%
============================================================

Gantt Chart:
+------------------------+------------------------+-------+-------+
|           P1           |           P2           |  P1   |  P2   |
+------------------------+------------------------+-------+-------+
0                        2                        4       5       6

Execution Timeline:
  [0 - 2: P1] [2 - 4: P2] [4 - 5: P1] [5 - 6: P2]
```

---

## Gantt Chart & Results Renderer

Batch 11 introduces an advanced, duration-aware text-based Gantt chart renderer (`GanttChart`) and a dedicated results formatting engine (`ResultsRenderer`):

### 1. Duration-Aware Gantt Chart Renderer (`GanttChart`)
- **Proportional Scaling**: Automatically scales block widths based on execution slice durations relative to the minimum segment duration.
- **Timestamp Alignment**: Timestamps on the timeline axis align precisely beneath block boundary characters (`+` or `|`), preventing numeric jitter or displacement.
- **Multi-Row Wrapping**: Automatically wraps wide schedules across multiple rows when the accumulated width exceeds `max_row_width` (default 80 columns), continuing timestamps cleanly on subsequent rows.
- **Explicit CPU Idle Intervals**: Unscheduled CPU time intervals are rendered explicitly with duration-scaled `Idle` blocks and exact start/end boundary markers.
- **Preemption & Slicing**: Seamlessly displays repeated process segments resulting from preemptive scheduling (SRTF, Round Robin, Preemptive Priority, MLQ, MLFQ).
- **Queue Metadata Support**: Visualizes assigned queue identifiers (e.g. `P1 [Q0]`) for Multilevel Queue simulations.
- **Border Styles**: Supports both standard ASCII (`+`, `-`, `|`) and clean Unicode box-drawing characters (`┌`, `─`, `┬`, `┐`, `│`, `└`, `┴`, `┘`).

### 2. Dedicated Results Presentation Engine (`ResultsRenderer`)
- `render_summary()`: Top-level simulation metrics (algorithm, process count, total time, busy/idle breakdown, CPU utilization).
- `render_process_table()`: Formatted tabular process states with dynamic columns (Priority, Queue ID) based on algorithm requirements.
- `render_aggregate_metrics()`: Formatted system-wide averages for turnaround, waiting, and response times.
- `render_timeline()`: Compact chronological string of execution segments (`[0 - 2: P1] [2 - 4: Idle] ...`).
- `render_gantt_chart()`: Seamlessly delegates to `GanttChart::render()` with configurable options.
- `render_full_report()`: Comprehensive presentation combining all five components into a cohesive simulation report (available as `std::string` or direct stream output to `std::ostream&`).

---


---

## CSV Workload Import and Results Export

Batch 12 delivers production-grade CSV import and export capabilities:

### 1. Workload CSV Schema
Standard format for importing process workloads:
```csv
process_id,arrival_time,burst_time,priority,queue_id
P1,0,5,2,Q_High
P2,1,3,1,Q_High
P3,2,8,3,Q_Low
```
- **Required Columns**: `process_id` (aliases: `pid`, `process`), `arrival_time` (aliases: `arrival`), `burst_time` (aliases: `burst`).
- **Optional Columns**: `priority` (aliases: `prio`, default 0), `queue_id` (aliases: `queue`, for MLQ bindings).
- **Validation**: Headers are matched case-insensitively with surrounding whitespace ignored. Unsupported columns and duplicate headers are rejected. Row-level validation provides 1-based error messages for missing IDs, duplicates, negative arrival times, non-positive burst times, non-integers, and row counts exceeding the 100 process limit. All-or-nothing semantics guarantee that either the entire workload imports or none of it does.

### 2. Process Metrics & Timeline Export
- **Process Metrics CSV**: Exports `process_id,arrival_time,burst_time,completion_time,turnaround_time,waiting_time,response_time,priority,queue_id` suitable for spreadsheet analysis.
- **Timeline CSV**: Exports `start_time,end_time,process_id,queue_id` with explicit `IDLE` intervals and preserved chronological order.
- **Formula Injection Mitigation**: Text fields (`process_id`, `queue_id`) beginning with `=`, `+`, `-`, or `@` are neutralized with an apostrophe prefix (`'`) at export boundaries. Numeric columns (`arrival_time`, `burst_time`, `priority`, etc.) remain pure numbers and in-memory results are unmodified.
- **Safe File Writing**: Export operations write to a temporary file (`.tmp`) before in-place transactional replacement (via `ReplaceFileW` on Windows), guarding against interrupted writes, preventing partial files, and preserving original files if replacement fails.

### 3. Sample CSV Files
- [`sample_workload.csv`](file:///examples/sample_workload.csv): Basic FCFS/SJF/SRTF/RR workload without priorities.
- [`sample_workload_with_priority.csv`](file:///examples/sample_workload_with_priority.csv): Workload with priority values for Priority scheduling.
- [`sample_workload_mlq.csv`](file:///examples/sample_workload_mlq.csv): Workload with static queue assignments for Multilevel Queue scheduling.

## Project Structure

```text
CPU Scheduling Simulator/
│
├── CMakeLists.txt              # Top-level CMake project configuration
├── CMakePresets.json           # Multi-configuration presets (MSVC x64 Debug/Release)
├── vcpkg.json                  # vcpkg manifest defining dependencies (GoogleTest)
├── .gitignore                  # Git ignore rules for build artifacts and IDE files
├── README.md                   # Project overview, algorithm guide, and instructions
├── BATCH_5_IMPLEMENTATION_REPORT.md  # Batch 5 implementation report
├── BATCH_6_IMPLEMENTATION_REPORT.md  # Batch 6 implementation report
├── BATCH_7_IMPLEMENTATION_REPORT.md  # Batch 7 implementation report
├── BATCH_8_IMPLEMENTATION_REPORT.md  # Batch 8 implementation report
├── BATCH_9_IMPLEMENTATION_REPORT.md  # Batch 9 implementation report
├── BATCH_10_IMPLEMENTATION_REPORT.md # Batch 10 implementation report
├── BATCH_11_IMPLEMENTATION_REPORT.md # Batch 11 implementation report
├── BATCH_12_IMPLEMENTATION_REPORT.md # Batch 12 implementation report
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
│       ├── rr_scheduler.hpp     # Preemptive Round Robin scheduling engine
│       ├── priority_scheduler.hpp# Priority scheduling engine (Non-Preemptive & Preemptive)
│       ├── mlq_scheduler.hpp    # Multilevel Queue (MLQ) scheduling engine
│       ├── mlfq_scheduler.hpp   # Multilevel Feedback Queue (MLFQ) scheduling engine
│       ├── scheduling_engine.hpp# Unified scheduling engine & central dispatcher
│       ├── cli.hpp              # Interactive CLI controller and stream formatters
│       ├── results_renderer.hpp # Results presentation engine (Summary, Tables, Metrics)
│       ├── gantt_chart.hpp      # Duration-aware Gantt chart renderer (ASCII/Unicode)
│       ├── csv_parser.hpp       # RFC 4180 compliant CSV parser
│       ├── workload_importer.hpp# CSV workload importer & validation diagnostics
│       └── csv_exporter.hpp     # Spreadsheet-safe CSV exporter & formula injection mitigation
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
│   ├── rr_scheduler.cpp
│   ├── priority_scheduler.cpp
│   ├── mlq_scheduler.cpp
│   ├── mlfq_scheduler.cpp
│   ├── scheduling_engine.cpp
│   ├── cli.cpp
│   ├── cli_main.cpp
│   ├── results_renderer.cpp
│   ├── gantt_chart.cpp
│   ├── csv_parser.cpp
│   ├── workload_importer.cpp
│   └── csv_exporter.cpp
│
├── tests/
│   ├── CMakeLists.txt          # GoogleTest suite target
│   ├── test_process.cpp        # Tests for process data model (7 tests)
│   ├── test_process_validator.cpp # Tests for validation logic (16 tests)
│   ├── test_fcfs_scheduler.cpp # Tests for FCFS scheduling engine (34 tests)
│   ├── test_sjf_scheduler.cpp  # Tests for SJF scheduling engine (52 tests)
│   ├── test_srtf_scheduler.cpp # Tests for SRTF scheduling engine (63 tests)
│   ├── test_rr_scheduler.cpp   # Tests for Round Robin scheduling engine (54 tests)
│   ├── test_priority_scheduler.cpp # Tests for Priority scheduling engines (68 tests)
│   ├── test_mlq_scheduler.cpp  # Tests for Multilevel Queue scheduling engine (77 tests)
│   ├── test_mlfq_scheduler.cpp # Tests for Multilevel Feedback Queue scheduling engine (77 tests)
│   ├── test_scheduling_engine.cpp # Tests for Unified Scheduling Engine & Dispatcher (39 tests)
│   ├── test_cli.cpp            # Tests for Interactive CLI Controller & Workflows (24 tests)
│   ├── test_gantt_chart.cpp    # Tests for Advanced Gantt Chart Visualization (13 tests)
│   ├── test_results_renderer.cpp # Tests for Results Presentation Engine (8 tests)
│   ├── test_csv_parser.cpp     # Tests for RFC 4180 CSV parser (11 tests)
│   ├── test_csv_workload_importer.cpp # Tests for CSV Workload Importer (17 tests)
│   ├── test_csv_results_exporter.cpp  # Tests for CSV Exporter & Formula Injection (8 tests)
│   └── test_cli_csv.cpp        # Tests for CLI CSV workflows (3 tests)
│
├── docs/
│   └── ARCHITECTURE.md         # Component boundaries and lifecycle documentation
│
└── examples/
    ├── CMakeLists.txt          # Example runner target
    └── basic_usage.cpp         # Demonstrates scheduling algorithms side-by-side
```
