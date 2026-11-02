# CPU Scheduling Simulator — Batch 6 Implementation Report: Priority Scheduling

**Date:** November 2, 2026
**Project:** Modern C++20 CPU Scheduling Simulator
**Milestone:** Batch 6 — Preemptive and Non-Preemptive Priority Scheduling Implementation
**Audience:** Technical Reviewers & Independent Verification Auditor
**Repository Directory:** `D:\Project\CPU Schedulling`
**Current Git Branch:** `main`
**Parent Commit (Batch 5):** `9c22b39da30f16fbfd445f8f62576808b974adfc`

---

## A. Implementation Summary

Batch 6 introduces two complete, robust, and mathematically sound CPU scheduling algorithms to the simulator:
1. **Non-Preemptive Priority Scheduler (`cpu_scheduler::NonPreemptivePriorityScheduler`)**
2. **Preemptive Priority Scheduler (`cpu_scheduler::PreemptivePriorityScheduler`)**

Both algorithms:
- Strictly observe the mandatory priority convention: **smaller numerical value represents strictly higher scheduling priority** (e.g., priority 1 > priority 2, and negative values like -5 > 0).
- Enforce deterministic tie-breaking:
  1. Priority value (smaller wins).
  2. Arrival time (earlier wins).
  3. Original workload input index (earlier wins).
- Preemptive scheduling preemption rule: a newly arriving process preempts the running process **if and only if** its priority is **strictly higher** (`new_proc.priority < running_proc.priority`). Equal-priority and lower-priority arrivals **never** preempt the active process.
- All original input specifications remain immutable.
- Preempted processes preserve remaining execution time and return to the ready pool.
- Schedulers are event-driven, jumping directly between discrete scheduling events (arrivals, completions, preemptions) rather than stepping single time units.
- Arithmetic operations are protected with `safe_add` to prevent signed 64-bit integer overflow.

---

## B. Process Model Changes

1. **Priority Field Representation**:
   - `Process` already contains `int priority{0}` from the Batch 1 architecture design.
   - Default value is `0`.
   - Constructor `Process(std::string process_id, Time arrival, Time burst, int prio = 0)` supports backwards compatibility with 3-argument invocations.
   - Zero and negative priorities are fully valid (e.g., -10, -5, 0, 100).
   - Priority comparisons use safe relational operators (`>`) without subtraction to eliminate signed integer overflow risks with extreme priority values.
2. **Dynamic Process State (`ProcessState`)**:
   - Copies `priority` from `Process` during `ProcessState::from_process(proc)`.
   - Dynamic simulation state tracks `remaining_time`, `start_time` (first CPU allocation), `completion_time`, `turnaround_time`, `waiting_time`, and `response_time`.
3. **Workload Validation**:
   - Validated through `ProcessValidator::validate(processes)`.
   - Verifies non-empty workload, non-empty/non-whitespace IDs, unique IDs (case-insensitive and trimmed), non-negative arrival times, and strictly positive burst times.
   - Zero and negative priority values are permitted without artificial constraints.

---

## C. Architecture & Design

### 1. New Files
- `include/cpu_scheduler/priority_scheduler.hpp`: Public headers for `NonPreemptivePriorityScheduler` and `PreemptivePriorityScheduler`.
- `src/priority_scheduler.cpp`: Implementations of both scheduling engines.
- `tests/test_priority_scheduler.cpp`: Comprehensive GoogleTest suite (68 new unit tests).

### 2. Modified Files
- `CMakeLists.txt`: Integrated `src/priority_scheduler.cpp` into `cpu_scheduler_lib`.
- `tests/CMakeLists.txt`: Integrated `test_priority_scheduler.cpp` into `cpu_scheduler_tests`.
- `examples/basic_usage.cpp`: Extended example to demonstrate all 6 algorithms side-by-side on a priority workload.
- `README.md`: Updated with Batch 6 status, Priority scheduling rules, comparison tables, and project structure.
- `docs/ARCHITECTURE.md`: Documented priority direction, tie-breaking, preemption mechanics, starvation limitations, and architectural diagrams.

### 3. Public API Parity
Both classes provide identical, uniform public interfaces:
```cpp
class NonPreemptivePriorityScheduler {
public:
    constexpr NonPreemptivePriorityScheduler() = default;
    [[nodiscard]] ScheduleOutcome schedule(std::span<const Process> processes) const;
    [[nodiscard]] ScheduleOutcome schedule(const std::vector<Process>& processes) const;
    [[nodiscard]] static ScheduleOutcome run(std::span<const Process> processes);
    [[nodiscard]] static ScheduleOutcome run(const std::vector<Process>& processes);
};

class PreemptivePriorityScheduler {
public:
    constexpr PreemptivePriorityScheduler() = default;
    [[nodiscard]] ScheduleOutcome schedule(std::span<const Process> processes) const;
    [[nodiscard]] ScheduleOutcome schedule(const std::vector<Process>& processes) const;
    [[nodiscard]] static ScheduleOutcome run(std::span<const Process> processes);
    [[nodiscard]] static ScheduleOutcome run(const std::vector<Process>& processes);
};
```

---

## D. Non-Preemptive Priority Scheduling Algorithm

### 1. Scheduling Procedure
1. Workload validated via `ProcessValidator::validate`.
2. Unarrived processes indexed and stably sorted by `arrival_time`.
3. Ready queue initialized as a min-heap priority queue ordered by:
   - Priority (smaller value first).
   - Arrival time (earlier first).
   - Input index (earlier first).
4. While ready queue is non-empty or unarrived processes remain:
   - If ready queue is empty, fast-forward time to next arrival and emit an idle segment if `next_arrival > current_time`.
   - Admit all processes with $arrival\_time \le current\_time$.
   - Pop highest-priority candidate from ready queue.
   - Run process continuously to completion ($completion\_time = current\_time + burst\_time$).
   - Record execution segment, update busy time and process state metrics.
   - Advance $current\_time = completion\_time$.
   - Admit all processes arriving during this execution into ready queue.
5. Compute system metrics (averages, utilization).

### 2. Hand-Calculated Mandatory Example (Section 6.3)
Workload:
- P1: Arrival 0, Burst 5, Priority 3
- P2: Arrival 1, Burst 3, Priority 1
- P3: Arrival 2, Burst 2, Priority 2
- P4: Arrival 3, Burst 1, Priority 1

Timeline:
- $t=0$: Only P1 ready $\rightarrow$ P1 runs $[0, 5]$.
- $t=5$: P2, P3, P4 have arrived. Ready queue: P2(prio 1, arr 1), P4(prio 1, arr 3), P3(prio 2, arr 2).
  - P2 vs P4: Equal priority (1); P2 arrived earlier ($1 < 3$). P2 runs $[5, 8]$.
- $t=8$: P4 vs P3 $\rightarrow$ P4 runs $[8, 9]$.
- $t=9$: P3 runs $[9, 11]$.

Metrics:
- P1: $ST=0, CT=5, TAT=5, WT=0, RT=0$
- P2: $ST=5, CT=8, TAT=7, WT=4, RT=4$
- P3: $ST=9, CT=11, TAT=9, WT=7, RT=7$
- P4: $ST=8, CT=9, TAT=6, WT=5, RT=5$
- Aggregate: $TAT_{\text{avg}} = 6.75$, $WT_{\text{avg}} = 4.00$, $RT_{\text{avg}} = 4.00$, Utilization: $100.0\%$.

---

## E. Preemptive Priority Scheduling Algorithm

### 1. Scheduling Procedure
1. Workload validated via `ProcessValidator::validate`.
2. Event-driven simulation tracks `active` process (`slice_start_time`, `remaining_time`).
3. Discrete event processing:
   - **Step A**: Admit all arrivals up to $current\_time$ into the ready priority queue.
   - **Step B**: If CPU idle, advance to next arrival (recording idle segment if gap exists) or dispatch highest-priority process from ready queue.
   - **Step C (Preemption Check)**: If active process is running and `ready_queue.top().priority < active.priority`:
     - Emit execution segment $[slice\_start\_time, current\_time]$.
     - Requeue active process with its remaining burst.
     - Context-switch to strictly higher-priority candidate.
   - **Step D (Event Advancement)**:
     - Project completion at $current\_time + active.remaining\_time$.
     - If next arrival occurs before completion ($next\_arrival < projected\_completion$):
       - Advance time to $next\_arrival$.
       - Deduct elapsed duration from active process's remaining burst.
       - Accumulate busy time.
       - Loop back to admit arrivals and evaluate preemption.
     - If no arrival occurs before completion:
       - Run active process to completion at $projected\_completion$.
       - Emit execution segment $[slice\_start\_time, projected\_completion]$.
       - Record metrics and mark completed.
       - Clear active process.

### 2. Hand-Calculated Mandatory Example (Section 7.4)
Workload:
- P1: Arrival 0, Burst 7, Priority 3
- P2: Arrival 2, Burst 4, Priority 1
- P3: Arrival 4, Burst 2, Priority 2
- P4: Arrival 5, Burst 3, Priority 1

Timeline:
- $t=0$: P1 starts $[0, \dots]$.
- $t=2$: P2 arrives ($prio=1 < 3$). Preempts P1! P1 slice $[0, 2]$ recorded ($rem=5$). P2 starts.
- $t=4$: P3 arrives ($prio=2 > 1$). Does not preempt P2. P2 continues.
- $t=5$: P4 arrives ($prio=1 == 1$). Does not preempt P2. P2 continues.
- $t=6$: P2 completes. Slice $[2, 6]$ recorded.
- $t=6$: Ready queue has P4(prio 1), P3(prio 2), P1(prio 3). P4 dispatched, runs $[6, 9]$ (completes).
- $t=9$: P3 dispatched, runs $[9, 11]$ (completes).
- $t=11$: P1 resumes, runs $[11, 16]$ (completes).

Gantt Chart:
`| P1 | P2 | P4 | P3 | P1 |`
`0    2    6    9    11   16`

Metrics:
- P1: $ST=0, CT=16, TAT=16, WT=9, RT=0$
- P2: $ST=2, CT=6, TAT=4, WT=0, RT=0$
- P3: $ST=9, CT=11, TAT=7, WT=5, RT=5$
- P4: $ST=6, CT=9, TAT=4, WT=1, RT=1$
- Aggregate: $TAT_{\text{avg}} = 7.75$, $WT_{\text{avg}} = 3.75$, $RT_{\text{avg}} = 1.50$, Utilization: $100.0\%$.

---

## F. Mandatory Additional Hand-Calculated Scenarios

### Scenario A — Equal Priority, Different Arrival Times
- Workload: P1(0, 3, prio 2), P2(1, 2, prio 1), P3(2, 4, prio 1).
- **Non-Preemptive**:
  - P1(0-3) $\rightarrow$ P2(3-5) $\rightarrow$ P3(5-9).
  - P1: CT=3, TAT=3, WT=0, RT=0.
  - P2: CT=5, TAT=4, WT=2, RT=2.
  - P3: CT=9, TAT=7, WT=3, RT=3.
  - Averages: $TAT_{\text{avg}} \approx 4.67$, $WT_{\text{avg}} \approx 1.67$.
- **Preemptive**:
  - P1(0-1) $\rightarrow$ P2(1-3) $\rightarrow$ P3(3-7) $\rightarrow$ P1(7-9).
  - P1: ST=0, CT=9, TAT=9, WT=6, RT=0.
  - P2: ST=1, CT=3, TAT=2, WT=0, RT=0.
  - P3: ST=3, CT=7, TAT=5, WT=1, RT=1.
  - Averages: $TAT_{\text{avg}} = 5.33$, $WT_{\text{avg}} = 2.33$.

### Scenario B — Multiple Preemptions
- Workload: P1(0, 10, prio 5), P2(2, 5, prio 3), P3(4, 3, prio 1), P4(6, 2, prio 2).
- **Preemptive Timeline**:
  `P1(0-2) -> P2(2-4) -> P3(4-7) -> P4(7-9) -> P2(9-12) -> P1(12-20)`
- Metrics:
  - P1: CT=20, TAT=20, WT=10, RT=0
  - P2: CT=12, TAT=10, WT=5, RT=0
  - P3: CT=7, TAT=3, WT=0, RT=0
  - P4: CT=9, TAT=3, WT=1, RT=1

### Scenario C — Equal-Priority Arrival While Running
- Workload: P1(0, 6, prio 2), P2(2, 2, prio 2), P3(3, 1, prio 1).
- **Preemptive Timeline**:
  - At $t=2$: P2 arrives ($prio=2 == 2$). Does NOT preempt P1.
  - At $t=3$: P3 arrives ($prio=1 < 2$). Preempts P1!
  - `P1(0-3) -> P3(3-4) -> P1(4-7) -> P2(7-9)`.
  - Metrics:
    - P1: CT=7, TAT=7, WT=1, RT=0
    - P2: CT=9, TAT=7, WT=5, RT=5
    - P3: CT=4, TAT=1, WT=0, RT=0

### Scenario D — Idle Intervals
- Workload: P1(4, 3, prio 2), P2(12, 2, prio 1).
- Timeline (identical for both): `Idle(0-4) -> P1(4-7) -> Idle(7-12) -> P2(12-14)`.
- Total time: 14, Busy time: 5, Idle time: 9. Utilization: $35.71\%$.

### Scenario E — Negative Priorities
- Workload: P1(0, 3, prio 0), P2(0, 2, prio -5), P3(0, 1, prio 3).
- Order: P2(-5) $\rightarrow$ P1(0) $\rightarrow$ P3(3).
- Timeline (identical for both): `P2(0-2) -> P1(2-5) -> P3(5-6)`.

---

## G. Unit Testing & Regression Results

### 1. Test Distribution
- **Pre-existing tests**: 226 tests (Batch 1–5).
- **New tests**: **68 tests** in `tests/test_priority_scheduler.cpp`.
  - Non-Preemptive Priority Tests: 31 tests.
  - Preemptive Priority Tests: 37 tests.
- **Total test suite count**: **294 tests**.

### 2. Test Verification Standards
- Zero tests disabled, skipped, or conditionally bypassed.
- Hardcoded, independently hand-calculated expectations.
- Formal invariant checks asserted on all scenarios ($TAT = CT - AT$, $WT = TAT - BT$, $RT = ST - AT$, timeline contiguity, work conservation).

### 3. Build & Test Results
- **MSVC x64 Compiler Warnings**: **0 warnings** with `/W4 /permissive-`.
- **Debug Configuration**:
  ```text
  100% tests passed, 0 tests failed out of 294
  Total Test time (real) = 12.46 sec
  ```
- **Release Configuration**:
  ```text
  100% tests passed, 0 tests failed out of 294
  Total Test time (real) = 10.40 sec
  ```

---

## H. Complexity Analysis

### 1. Non-Preemptive Priority Scheduling
- **Time Complexity**: $O(n \log n)$
  - Sorting $n$ unarrived processes: $O(n \log n)$.
  - Each process is inserted into `std::priority_queue` exactly once: $n \times O(\log n) = O(n \log n)$.
  - Each process is popped from `std::priority_queue` exactly once: $n \times O(\log n) = O(n \log n)$.
  - Exactly $n$ process execution segments are emitted.
- **Auxiliary Space Complexity**: $O(n)$
  - Auxiliary structures (`unarrived`, `states`, `ready_queue`, and `timeline`) each require $O(n)$ storage.

### 2. Preemptive Priority Scheduling
- **Time Complexity**: $O(n \log n)$
  - Initial sort of $n$ unarrived processes: $O(n \log n)$.
  - Event-driven simulation jumps between discrete events: process arrivals and completions.
  - **Preemption Bound**: A preemption can occur *only* when a new process arrives with strictly higher priority than the currently running process. Because there are at most $n$ process arrivals in total, at most $n$ preemptions can ever occur during the simulation.
  - **Total Events & Queue Operations**:
    - Initial queue pushes: at most $n$.
    - Requeue pushes after preemption: at most $n$.
    - Total priority queue operations: at most $2n$ pushes and $2n$ pops.
    - Each operation on a heap of size $\le n$ takes $O(\log n)$ time.
    - Total simulation time: $O(n \log n)$.
- **Auxiliary Space Complexity**: $O(n)$
  - At most $n$ processes in ready queue: $O(n)$.
  - Number of execution segments in `timeline`: bounded by at most $2n - 1$, which is strictly $O(n)$.

---

## I. Documentation & Examples

- `README.md`: Updated with Batch 6 milestones, detailed priority scheduling sections, comparison tables, and full project structure tree.
- `docs/ARCHITECTURE.md`: Documented priority scheduling components, priority direction convention, deterministic tie-breaking rules, preemption conditions, and starvation notes.
- `examples/basic_usage.cpp`: Expanded to run all 6 implemented algorithms (FCFS, SJF, SRTF, RR, Non-Preemptive Priority, Preemptive Priority) side-by-side on an identical workload with complete Gantt charts and metrics tables.

---

## J. Git Evidence

- **Branch**: `main`
- **Parent Commit (Batch 5)**: `9c22b39da30f16fbfd445f8f62576808b974adfc`
- **Intended Batch 6 Commit Message**: `feat: implement preemptive and non-preemptive priority scheduling`
- **Files Created**:
  - `include/cpu_scheduler/priority_scheduler.hpp`
  - `src/priority_scheduler.cpp`
  - `tests/test_priority_scheduler.cpp`
  - `BATCH_6_IMPLEMENTATION_REPORT.md`
- **Files Modified**:
  - `CMakeLists.txt`
  - `tests/CMakeLists.txt`
  - `examples/basic_usage.cpp`
  - `README.md`
  - `docs/ARCHITECTURE.md`

---

## K. Known Limitations

- **Process Starvation**: Fixed-priority scheduling does not adjust priority over time. If high-priority processes arrive continuously, lower-priority processes will wait indefinitely.
- **Absence of Priority Aging**: Priority aging (dynamically elevating priority based on ready-queue wait time) is intentionally out of scope for Batch 6 and will be implemented in a subsequent batch.
- **Interactive Workload Entry**: Workloads are currently configured programmatically. Interactive CLI execution is planned for Batch 7.

---

## L. Final Completion Status

**ALL BATCH 6 ACCEPTANCE CRITERIA MET AND VERIFIED.**
The implementation is mathematically correct, deterministic, overflow-safe, 100% covered by passing regression tests across Debug and Release presets, and ready for independent verification audit.
