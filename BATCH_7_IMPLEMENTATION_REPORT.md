# CPU Scheduling Simulator — Batch 7: Multilevel Queue (MLQ) Scheduling Implementation Report

**Author**: Assistant  
**Date**: November 2, 2026  
**Commit Message**: `feat: implement multilevel queue CPU scheduling`  
**Parent Commit**: `a0998f8ec48248aa87d75bac99e35aa27da93972` (Batch 6)  
**Target Branch**: `main`  

---

## 1. Executive Summary

Batch 7 delivers a comprehensive, robust, deterministic, and overflow-safe **Multilevel Queue (MLQ)** CPU scheduling engine to the C++20 CPU Scheduling Simulator.

Key achievements of Batch 7:
1. **Multilevel Queue Architecture**: Implemented multiple independent ready queues, each configured with an individual identifier, queue-level scheduling priority, internal scheduling policy, and optional quantum parameters.
2. **Permanent Process-to-Queue Assignment**: Processes are bound statically and permanently to a designated queue (`MlqProcessBinding`). As mandated, processes never migrate between queues, and dynamic feedback/aging (reserved for MLFQ) is strictly excluded.
3. **Decoupled Queue Priority vs. Process Priority**: Queue priority ($0 < 1 < 2$) strictly governs inter-queue CPU allocation. Process priority strictly governs intra-queue selection for priority-based internal algorithms (`NonPreemptivePriority` and `PreemptivePriority`).
4. **Configurable Inter-Queue Scheduling Policies**:
   - `InterQueuePolicy::FixedPriorityPreemptive`: The highest-priority ready queue always runs; higher-priority arrivals immediately preempt lower-priority queues. Equal-priority queues break ties deterministically by declaration order.
   - `InterQueuePolicy::RoundRobin`: Cycles fairly through non-empty queues using an inter-queue time quantum ($Q_{\text{inter}}$), skipping empty queues instantaneously without wasting simulation time.
5. **Intra-Queue Scheduling Integration**: Fully supports 6 distinct internal policies per queue:
   - First Come, First Served (`QueueAlgorithm::Fcfs`)
   - Shortest Job First non-preemptive (`QueueAlgorithm::Sjf`)
   - Shortest Remaining Time First preemptive (`QueueAlgorithm::Srtf`)
   - Round Robin (`QueueAlgorithm::RoundRobin`) with internal quantum $q_{\text{internal}}$
   - Non-Preemptive Priority (`QueueAlgorithm::NonPreemptivePriority`)
   - Preemptive Priority (`QueueAlgorithm::PreemptivePriority`)
6. **Two-Level Quantum Interaction**: Seamlessly handles nested time-slicing where an internal queue runs Round Robin while inter-queue scheduling is also Round Robin.
7. **Comprehensive Unit Testing**: Added **77 new GoogleTest unit tests** covering configuration, validation, both inter-queue policies, all mandatory hand-calculated Scenarios A through H, two-level quantum interactions, and boundary conditions.
8. **Test Regression**: All **371 tests** (294 baseline + 77 new) pass cleanly in both **Debug** and **Release** builds with **zero compiler warnings** under MSVC `/W4 /permissive-`.

---

## 2. Repository Baseline

Prior to starting Batch 7 implementation, the repository state was inspected and verified:
- **Active Branch**: `main`
- **Baseline HEAD Commit**: `a0998f8ec48248aa87d75bac99e35aa27da93972` (Batch 6)
- **Baseline Parent**: `9c22b39da30f16fbfd445f8f62576808b974adfc` (Batch 5)
- **Tracked Working Tree**: Clean; all existing 294 unit tests passing in Debug and Release.

---

## 3. Queue and Workload Data Model

The MLQ data models are defined in `include/cpu_scheduler/mlq_scheduler.hpp` and integrated with `include/cpu_scheduler/process_state.hpp`:

### 3.1 Data Structures

```cpp
enum class QueueAlgorithm {
    Fcfs,
    Sjf,
    Srtf,
    RoundRobin,
    NonPreemptivePriority,
    PreemptivePriority
};

enum class InterQueuePolicy {
    FixedPriorityPreemptive,
    RoundRobin
};

struct MlqQueueConfig {
    std::string id{};
    int priority{0};
    QueueAlgorithm algorithm{QueueAlgorithm::Fcfs};
    Time internal_quantum{1};
    std::optional<Time> inter_queue_quantum{std::nullopt};
};

struct MlqProcessBinding {
    Process process{};
    std::string queue_id{};
};

struct MlqWorkload {
    std::vector<MlqQueueConfig> queues{};
    std::vector<MlqProcessBinding> processes{};
    InterQueuePolicy inter_queue_policy{InterQueuePolicy::FixedPriorityPreemptive};
    Time default_inter_queue_quantum{2};
};
```

### 3.2 Backward-Compatible `ProcessState` Extension

`ProcessState` (`include/cpu_scheduler/process_state.hpp`) was extended with a non-breaking default field:
```cpp
std::string queue_id{};
```
This records the canonical assigned queue identifier for each process, verified across all MLQ simulations.

---

## 4. Configuration and Input Validation

Input validation is performed by `MlqScheduler::schedule` before simulation begins:
1. **Empty Workload Validation**: Rejects workloads with 0 processes or 0 queues (`ValidationErrorCode::EmptyWorkload`).
2. **Queue ID Validation**: Every configured queue must have a non-empty, non-whitespace identifier (`ValidationErrorCode::EmptyProcessId`).
3. **Queue ID Uniqueness**: Queue identifiers must be unique case-insensitively (`ValidationErrorCode::DuplicateProcessId`).
4. **Queue Binding Validation**: Every process must have a non-empty queue assignment that matches a configured queue ID case-insensitively (`ValidationErrorCode::ProcessNotFound`).
5. **Quantum Validation**:
   - For queues configured with `QueueAlgorithm::RoundRobin`, `internal_quantum` must be strictly positive ($> 0$).
   - When `InterQueuePolicy::RoundRobin` is selected, `default_inter_queue_quantum` and any queue-specific `inter_queue_quantum` overrides must be strictly positive ($> 0$).
6. **Workload Invariant Validation**: The process collection is validated via `ProcessValidator::validate` ensuring unique process IDs, non-negative arrival times, and positive burst times.

---

## 5. Fixed-Priority Inter-Queue Scheduling Engine

Under `InterQueuePolicy::FixedPriorityPreemptive`:
1. **Priority Rule**: Smaller numerical queue priority denotes higher scheduling preference ($0 < 1 < 2$).
2. **Active Queue Selection**: At each simulation step, the engine inspects all queues with ready processes. The queue with the lowest numerical priority is selected.
3. **Deterministic Tie-Breaking**: If two or more ready queues share identical queue priority, the queue defined earliest in the workload configuration sequence is chosen.
4. **Inter-Queue Preemption**: If the CPU is executing a process in queue $Q_B$, and a process arrives in queue $Q_A$ with $\text{priority}(Q_A) < \text{priority}(Q_B)$, the CPU immediately halts $Q_B$'s active process, saves its progress, and allocates the CPU to $Q_A$.
5. **Starvation Invariant**: Lower-priority queues are starved as long as higher-priority queues have runnable processes. Once higher-priority queues become empty, lower-priority queues resume seamlessly.

---

## 6. Round Robin Inter-Queue Scheduling Engine

Under `InterQueuePolicy::RoundRobin`:
1. **Cyclic Rotation**: The engine cycles through the configured queues in declaration order ($0, 1, \dots, K-1, 0, \dots$).
2. **Inter-Queue Time Quantum ($Q_{\text{inter}}$)**: Each active queue is allocated up to $Q_{\text{inter}}$ continuous simulation time units.
3. **Empty Queue Skipping**: If a queue has no ready processes when its turn arrives, it is skipped with zero time progression, immediately granting the turn to the next non-empty queue.
4. **Early Queue Depletion**: If all processes in the active queue finish or are blocked before the inter-queue quantum expires, the remaining quantum is forfeited and the turn advances to the next queue immediately.
5. **Fairness**: Every continuously non-empty queue receives equal CPU opportunity over time.

---

## 7. Intra-Queue Policy Integration & Two-Level Quanta

Each queue maintains its own internal ready state:
- **FCFS / Round Robin**: Managed via a strict FIFO `ready_deque`.
- **SJF / SRTF / Priority**: Managed via a `ready_pool` evaluated dynamically with three-tier tie-breaking.

### Two-Level Quantum Interaction
When an internal queue uses Round Robin with quantum $q_{\text{internal}}$ and the inter-queue scheduler uses Round Robin with quantum $Q_{\text{inter}}$:
- Both slice counters count down concurrently during an execution slice $\Delta t$.
- If $q_{\text{internal}}$ reaches 0 first, the current process is rotated to the back of the queue, and the next process in that queue executes for the remainder of $Q_{\text{inter}}$.
- If $Q_{\text{inter}}$ reaches 0 first, the queue's turn ends, and the current process retains its remaining internal quantum for when that queue is visited next.
- If the same process runs across quantum refreshes without preemption or intervening work, timeline normalization collapses consecutive segments into a single continuous segment.

---

## 8. Mandatory Hand-Calculated Scenarios and Metrics

All mandatory scenarios specified in Batch 7 requirements were independently hand-calculated, implemented, and verified in `tests/test_mlq_scheduler.cpp`.

### Scenario A — Fixed-Priority Inter-Queue Preemption
- **Queues**: Q0 (pri 0, FCFS), Q1 (pri 1, FCFS).
- **Processes**: P1 (AT=0, BT=8, Q1), P2 (AT=2, BT=3, Q0).
- **Timeline**: `P1(0-2) -> P2(2-5) -> P1(5-11)`
- **Metrics**:
  - P1: CT=11, TAT=11, WT=3, RT=0
  - P2: CT=5, TAT=3, WT=0, RT=0
  - System: Busy=11, Idle=0, Total=11, Util=100.0%

### Scenario B — Fixed-Priority Queue Remains Active
- **Queues**: Q0 (pri 0, FCFS), Q1 (pri 1, FCFS).
- **Processes**: P1 (AT=0, BT=5, Q0), P2 (AT=1, BT=3, Q1), P3 (AT=2, BT=2, Q1).
- **Timeline**: `P1(0-5) -> P2(5-8) -> P3(8-10)`
- **Metrics**:
  - P1: CT=5, TAT=5, WT=0, RT=0
  - P2: CT=8, TAT=7, WT=4, RT=4
  - P3: CT=10, TAT=8, WT=6, RT=6

### Scenario C — Round Robin Between Queues
- **Inter-Queue Quantum**: 2.
- **Queues**: Q0 (FCFS), Q1 (FCFS).
- **Processes**: P1 (AT=0, BT=5, Q0), P2 (AT=0, BT=4, Q1).
- **Timeline**: `P1(0-2) -> P2(2-4) -> P1(4-6) -> P2(6-8) -> P1(8-9)`
- **Metrics**:
  - P1: CT=9, TAT=9, WT=4, RT=0
  - P2: CT=8, TAT=8, WT=4, RT=2

### Scenario D — Empty Queue Skipping & Idle Fast-Forward
- **Inter-Queue Quantum**: 2.
- **Queues**: Q0 (FCFS), Q1 (FCFS).
- **Processes**: P1 (AT=0, BT=2, Q0), P2 (AT=5, BT=3, Q1).
- **Timeline**: `P1(0-2) -> Idle(2-5) -> P2(5-8)`
- **Metrics**:
  - P1: CT=2, TAT=2, WT=0, RT=0
  - P2: CT=8, TAT=3, WT=0, RT=0
  - System: Busy=5, Idle=3, Total=8, Util=62.5%

### Scenario E — Internal RR and Inter-Queue Fixed Priority
- **Queues**: Q0 (pri 0, RR $q=2$), Q1 (pri 1, FCFS).
- **Processes**: P1 (AT=0, BT=5, Q0), P2 (AT=0, BT=3, Q0), P3 (AT=0, BT=4, Q1).
- **Timeline**: `P1(0-2) -> P2(2-4) -> P1(4-6) -> P2(6-7) -> P1(7-8) -> P3(8-12)`
- **Metrics**:
  - P1: CT=8, TAT=8, WT=3, RT=0
  - P2: CT=7, TAT=7, WT=4, RT=2
  - P3: CT=12, TAT=12, WT=8, RT=8

### Scenario F — Simultaneous Queue Activation
- **Queues**: Q0 (pri 0, FCFS), Q1 (pri 1, FCFS).
- **Processes**: P1 (AT=0, BT=3, Q1), P2 (AT=0, BT=2, Q0).
- **Timeline**: `P2(0-2) -> P1(2-5)`

### Scenario G — Lower-Priority Queue Resumption (Multiple Preemptions)
- **Queues**: Q0 (pri 0, FCFS), Q1 (pri 1, FCFS).
- **Processes**: P1 (AT=0, BT=10, Q1), P2 (AT=2, BT=2, Q0), P3 (AT=6, BT=2, Q0).
- **Timeline**: `P1(0-2) -> P2(2-4) -> P1(4-6) -> P3(6-8) -> P1(8-14)`
- **Metrics**:
  - P1: CT=14, TAT=14, WT=4, RT=0
  - P2: CT=4, TAT=2, WT=0, RT=0
  - P3: CT=8, TAT=2, WT=0, RT=0

### Scenario H — Equal Queue Priority Deterministic Order
- **Queues**: Q0 (pri 1, FCFS), Q1 (pri 1, FCFS).
- **Processes**: P1 (AT=0, BT=3, Q0), P2 (AT=0, BT=3, Q1).
- **Timeline**: `P1(0-3) -> P2(3-6)`

---

## 9. Testing Breakdown

The new test suite in `tests/test_mlq_scheduler.cpp` contains **77 GoogleTest unit tests**:

| Category | Count | Focus Areas |
|---|:---:|---|
| **Configuration and Validation** | 18 | Empty processes, empty queues, duplicate queue IDs, blank IDs, missing bindings, invalid internal quantum, invalid inter-queue quantum, negative arrival, zero burst, builder APIs. |
| **Fixed-Priority Inter-Queue Policy** | 21 | Scenarios A, B, F, G, H, cascading preemption across 3 queues, exact completion boundary arrivals, negative queue priorities, starvation behavior, idle gaps, utilization. |
| **Round Robin Inter-Queue Policy** | 20 | Scenarios C, D, 3-queue rotation, empty queue skipping, late arrivals, early quantum release, custom queue quanta, extreme $q=1$, large workloads, fairness. |
| **Internal Policy Integration** | 18 | Scenario E, internal FCFS, internal SJF, internal SRTF, internal Priority, two-level quantum interplay, mixed algorithms across queues, tie-breaking preservation, static vs instance equivalence. |
| **Total New Tests** | **77** | Exceeds minimum threshold of 70 tests. |

---

## 10. Debug and Release Regression Results

Both build presets were compiled and verified with CTest:

### 10.1 Debug Preset (`debug` / `debug-test`)
- **Compiler**: MSVC 19.44.35207 (`/W4 /permissive-`)
- **Compilation Result**: Succeeded with 0 warnings.
- **CTest Output**:
  ```
  100% tests passed out of 371
  Total Test time (real) = 11.78 sec
  ```

### 10.2 Release Preset (`release` / `release-test`)
- **Compiler**: MSVC 19.44.35207 (`/W4 /permissive-`)
- **Compilation Result**: Succeeded with 0 warnings.
- **CTest Output**:
  ```
  100% tests passed out of 371
  Total Test time (real) = 13.27 sec
  ```

---

## 11. Arithmetic Safety

The MLQ scheduling engine strictly implements signed 64-bit integer overflow protection using `safe_add`:
- Cumulative simulation time, busy time, idle time, and segment boundary calculations are guarded.
- Relational comparisons for priority use standard integer relations without difference subtraction:
  `priority_a < priority_b`
  avoiding underflow/overflow vulnerabilities when comparing extreme priority values (e.g., `INT_MIN` vs `INT_MAX`).

---

## 12. Complexity Analysis

Let:
- $N$ = total number of processes across all queues.
- $K$ = number of configured ready queues.
- $B$ = sum of all process burst times.
- $Q$ = inter-queue time quantum.
- $q$ = intra-queue Round Robin quantum.

### 12.1 Time Complexity
1. **Validation & Sorting**:
   - Initial validation and normalization takes $O(N + K)$ time.
   - Initial chronological arrival sorting takes $O(N \log N)$ time.
2. **Fixed-Priority Inter-Queue**:
   - Each arrival and completion event takes $O(K + \log N)$ to select the active queue and candidate process.
   - For non-preemptive internal algorithms, there are $O(N)$ scheduling events.
   - For preemptive algorithms (SRTF, Preemptive Priority), arrivals cause at most $N$ preemptions, giving $O(N \log N + K \cdot N)$ total time.
3. **Round Robin Inter-Queue**:
   - Total slice count is bounded by $O(\frac{B}{Q} + N)$.
   - Each queue advance checks at most $K$ queues in $O(K)$.
   - Total time is $O(N \log N + K \cdot \frac{B}{Q})$.

### 12.2 Auxiliary Space Complexity
- Ready queues and pools store $O(N)$ process indices.
- Process states and tracking structures require $O(N)$ auxiliary space.
- Timeline segments:
  - Fixed-Priority: at most $2N - 1$ process segments plus intervening idle segments $\implies O(N)$.
  - Round Robin: bounded by $O(N + \frac{B}{Q})$ normalized segments.

---

## 13. Documentation and Examples

1. **`examples/basic_usage.cpp`**:
   - Updated to demonstrate both MLQ Fixed-Priority Preemptive (with system, interactive, and batch queues) and MLQ Round Robin Inter-Queue policies.
   - Generates ASCII Gantt charts, per-process state tables, and utilization summaries.
2. **`README.md`**:
   - Updated Current Status to Batch 7.
   - Added Section 7 detailing Multilevel Queue scheduling policies, design invariants, two-level quantum behavior, and complexity.
3. **`docs/ARCHITECTURE.md`**:
   - Added `MlqScheduler` component to Mermaid architecture diagram.
   - Added Section 2.11 documenting MLQ component boundaries, permanent process-to-queue binding, and policy interactions.

---

## 14. Git Commit and Changed Files

### Changed Tracked Files
1. `CMakeLists.txt`: Added `src/mlq_scheduler.cpp` to `cpu_scheduler_lib`.
2. `include/cpu_scheduler/process_state.hpp`: Added `std::string queue_id{};` to `ProcessState`.
3. `tests/CMakeLists.txt`: Added `test_mlq_scheduler.cpp` to `cpu_scheduler_tests`.
4. `examples/basic_usage.cpp`: Added MLQ usage examples for both inter-queue policies.
5. `README.md`: Updated Batch 7 documentation and MLQ reference.
6. `docs/ARCHITECTURE.md`: Added MLQ architecture documentation and Mermaid diagram.

### Newly Created Files
1. `include/cpu_scheduler/mlq_scheduler.hpp`: MLQ public headers, configuration, and interfaces.
2. `src/mlq_scheduler.cpp`: MLQ simulation and validation implementation.
3. `tests/test_mlq_scheduler.cpp`: 77 comprehensive MLQ unit tests.
4. `BATCH_7_IMPLEMENTATION_REPORT.md`: This comprehensive implementation report.

---

## 15. Known Limitations

1. **Static Queue Assignment**: Processes remain permanently in their assigned queue; priority aging or migration across queues is intentionally excluded (as this is MLQ, not MLFQ).
2. **Starvation Under Fixed-Priority**: Lower-priority queues can starve indefinitely if higher-priority queues are continuously supplied with incoming workloads. To ensure bounded waiting, `InterQueuePolicy::RoundRobin` should be selected.

---

## 16. Final Completion Status

Batch 7 implementation is **COMPLETE** and fully verified:
- Multiple ready queues fully supported.
- Process-to-queue binding immutable and permanent.
- Fixed-Priority and Round Robin inter-queue policies functional.
- All 6 internal policies integrated and verified.
- 77 new tests passing (371 total tests passing in Debug and Release).
- 0 compiler warnings under `/W4 /permissive-`.
- Ready for independent verification audit.
