# CPU Scheduling Simulator — Batch 5 Implementation Report
### Round Robin (RR) CPU Scheduling Implementation

---

## A. Implementation Summary

Batch 5 introduces the preemptive **Round Robin (RR)** CPU scheduling algorithm to the modern C++20 CPU Scheduling Simulator.

Key features implemented:
- **Round Robin Scheduler (`RrScheduler`)**: Implemented under the `cpu_scheduler` namespace in `include/cpu_scheduler/rr_scheduler.hpp` and `src/rr_scheduler.cpp`.
- **Configurable Time Quantum**: Supports default quantum (2) or user-configured positive quantum via constructor or method override. Zero or negative quantum values are rejected with structured validation errors.
- **Strict FIFO Ready Queue**: Employs `std::deque` to maintain true FIFO rotation. Processes are executed for $\min(q, \text{remaining\_burst})$.
- **Quantum Expiration & Simultaneous Arrivals Ordering**: When a process completes its quantum, all unarrived processes with `arrival_time <= current_time` enter the ready queue in stable arrival/input order **before** the preempted process is appended to the tail of the ready queue.
- **No Unnecessary Context Switching / Timeline Normalization**: If a process's quantum expires but no other process is waiting in the ready queue, the process continues running. Adjacent execution segments for the same process are normalized into a continuous timeline segment.
- **CPU Idle Periods**: When the ready queue is empty, the clock advances directly to the next process arrival, recording an explicit idle segment.
- **Metrics Integrity**: Response Time ($RT$) strictly captures the first CPU start time ($RT = \text{First Start} - \text{Arrival Time}$), remaining invariant across subsequent quanta. Turnaround Time ($TAT = CT - AT$) and Waiting Time ($WT = TAT - BT$) accurately aggregate all intermediate ready-queue wait intervals.

---

## B. Architecture

### New Files Created (3):
1. `include/cpu_scheduler/rr_scheduler.hpp`: Class interface with configurable quantum and span-based scheduling methods.
2. `src/rr_scheduler.cpp`: Complete event-driven implementation with checked arithmetic and timeline normalization.
3. `tests/test_rr_scheduler.cpp`: 54 GoogleTest unit tests covering all required scenarios and edge cases.

### Modified Files (4):
1. `CMakeLists.txt`: Added `src/rr_scheduler.cpp` to `cpu_scheduler_lib`.
2. `tests/CMakeLists.txt`: Added `test_rr_scheduler.cpp` to `cpu_scheduler_tests`.
3. `examples/basic_usage.cpp`: Demonstrates Round Robin scheduling compared against FCFS, SJF, and SRTF.
4. `README.md` & `docs/ARCHITECTURE.md`: Documented Round Robin, algorithm comparison, updated project layout, and architecture diagrams.

### Public API:
```cpp
namespace cpu_scheduler {

class RrScheduler {
public:
    constexpr RrScheduler() noexcept;
    explicit constexpr RrScheduler(Time time_quantum) noexcept;

    [[nodiscard]] constexpr Time time_quantum() const noexcept;

    [[nodiscard]] ScheduleOutcome schedule(std::span<const Process> processes) const;
    [[nodiscard]] ScheduleOutcome schedule(const std::vector<Process>& processes) const;
    [[nodiscard]] ScheduleOutcome schedule(std::span<const Process> processes, Time time_quantum) const;
    [[nodiscard]] ScheduleOutcome schedule(const std::vector<Process>& processes, Time time_quantum) const;

    [[nodiscard]] static ScheduleOutcome run(std::span<const Process> processes, Time time_quantum);
    [[nodiscard]] static ScheduleOutcome run(const std::vector<Process>& processes, Time time_quantum);
};

} // namespace cpu_scheduler
```

### Shared Components Reused:
- `Process`: Immutable workload model.
- `ProcessValidator`: Input workload verification.
- `ProcessState`: Mutable execution and metric tracking.
- `ExecutionSegment`: Chronological timeline segments.
- `SchedulingResult` & `ScheduleOutcome`: Unified results and error reporting.
- `GanttChart`: Reused ASCII Gantt chart renderer.

---

## C. Algorithm Correctness

### 1. Ready Queue Semantics & Rotation
- Processes arriving at or before `current_time` are admitted into the ready queue.
- The process at the head is dispatched for $\text{slice} = \min(q, \text{remaining\_time})$.
- Clock is advanced by `slice`. Remaining burst is decremented by `slice`.

### 2. Simultaneous Arrivals at Quantum Boundary
- When a process finishes its quantum at time $T$, newly arrived processes with $\text{arrival\_time} \le T$ are pushed to the ready queue **before** the active process is requeued.
- If the active process has `remaining_time == 0`, it is completed and never requeued.

### 3. Timeline Normalization
- When consecutive allocations occur for the same process (e.g. when no competitor is ready), `append_normalized_segment` merges `[T1, T2]` and `[T2, T3]` into `[T1, T3]`.
- This ensures clean Gantt charts without artificial context-switch fragmentation.

### 4. Arithmetic Safety
- Time advancements use `safe_add(a, b, out)`.
- If additions exceed `std::numeric_limits<Time>::max()`, `ScheduleErrorKind::ArithmeticOverflow` is returned.
- Subtractions ($CT - AT$, $TAT - BT$, $ST - AT$) are strictly non-negative.
- Zero-division guards are applied to all floating-point averages and CPU utilization.

---

## D. Mandatory Scenarios (Scenarios A–H)

### Scenario A — Basic Round Robin (Quantum = 2)
- Workload: P1(0, 5), P2(0, 3), P3(0, 1)
- **Gantt Chart**: `P1(0-2) -> P2(2-4) -> P3(4-5) -> P1(5-7) -> P2(7-8) -> P1(8-9)`
- **Metrics**:
  - P1: CT 9, TAT 9, WT 4, RT 0
  - P2: CT 8, TAT 8, WT 5, RT 2
  - P3: CT 5, TAT 5, WT 4, RT 4

### Scenario B — Different Arrival Times (Quantum = 2)
- Workload: P1(0, 5), P2(2, 3), P3(4, 2)
- **Gantt Chart**: `P1(0-2) -> P2(2-4) -> P1(4-6) -> P3(6-8) -> P2(8-9) -> P1(9-10)`
- **Metrics**:
  - P1: CT 10, TAT 10, WT 5, RT 0
  - P2: CT 9,  TAT 7,  WT 4, RT 0
  - P3: CT 8,  TAT 4,  WT 2, RT 2

### Scenario C — CPU Idle Time (Quantum = 2)
- Workload: P1(3, 4), P2(10, 2)
- **Gantt Chart**: `IDLE(0-3) -> P1(3-7) -> IDLE(7-10) -> P2(10-12)`
- Normalized: Adjacent allocations for P1 (3–5 and 5–7) merge into `P1(3-7)`.
- **Metrics**: Total Time: 12, Busy Time: 6, Idle Time: 6, Utilization: 50.0%.

### Scenario D — Process Arrives Exactly at Quantum Expiration (Quantum = 2)
- Workload: P1(0, 5), P2(2, 2)
- P2 arrives at 2, admitted before P1 requeued.
- **Gantt Chart**: `P1(0-2) -> P2(2-4) -> P1(4-7)`
- Normalized: P1(4–6) and P1(6–7) merge into `P1(4-7)`.

### Scenario E — Process Completes Exactly at Quantum Expiration (Quantum = 2)
- Workload: P1(0, 2), P2(0, 4)
- **Gantt Chart**: `P1(0-2) -> P2(2-6)`
- P1 finishes at 2 and is never requeued. P2 runs 2–4 and 4–6, normalized to `P2(2-6)`.

### Scenario F — Single Process (Quantum = 2)
- Workload: P1(0, 7)
- **Gantt Chart**: `P1(0-7)`
- CT: 7, TAT: 7, WT: 0, RT: 0. One continuous execution segment.

### Scenario G — Quantum Larger Than Every Burst (Quantum = 10)
- Workload: P1(0, 3), P2(0, 2)
- **Gantt Chart**: `P1(0-3) -> P2(3-5)`
- Equivalent to FCFS scheduling.

### Scenario H — Quantum Equals Burst Time (Quantum = 4)
- Workload: P1(0, 4), P2(0, 4)
- **Gantt Chart**: `P1(0-4) -> P2(4-8)`
- Both processes complete in a single quantum without requeuing.

---

## E. Test Results

- **New Round Robin Tests**: **54 tests** (exceeding the 45-test requirement).
- **Total Registered Tests**: **226 tests** (172 baseline + 54 new).
- **Debug CTest**:
  - `ctest --preset debug-test`: **100% passed out of 226 tests** (0 failed).
- **Release CTest**:
  - `ctest --preset release-test`: **100% passed out of 226 tests** (0 failed).
- **Compiler Warnings**: **0 warnings** on MSVC x64 with `/W4 /permissive-`.
- **Skipped / Disabled Tests**: **0**.

---

## F. Git Evidence

- **Branch**: `main`
- **Previous Approved HEAD (Batch 4)**: `add169239d690069893ce46d1b5057989d690790`
- **Batch 5 Implementation Commit**: *To be committed*
- **Modified & Created Files**:
  - Created: `include/cpu_scheduler/rr_scheduler.hpp`
  - Created: `src/rr_scheduler.cpp`
  - Created: `tests/test_rr_scheduler.cpp`
  - Created: `BATCH_5_IMPLEMENTATION_REPORT.md`
  - Modified: `CMakeLists.txt`
  - Modified: `tests/CMakeLists.txt`
  - Modified: `examples/basic_usage.cpp`
  - Modified: `README.md`
  - Modified: `docs/ARCHITECTURE.md`

---

## G. Known Limitations

- **Interactive Time Quantum Input**: The time quantum is configured programmatically via C++ APIs. Interactive command-line quantum prompts and file-based scenario configurations will be introduced in Batch 7.
- **Remaining Algorithms**: Priority Scheduling (preemptive and non-preemptive) is scheduled for Batch 6.

---

## H. Final Status

**COMPLETE AND READY FOR INDEPENDENT AUDIT.**  
All Batch 5 requirements, algorithm invariants, mandatory scenarios (A–H), integration checks, and documentation deliverables have been fully implemented and verified.
