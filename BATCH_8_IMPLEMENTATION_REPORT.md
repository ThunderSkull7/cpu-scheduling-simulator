# CPU Scheduling Simulator — Batch 8 Implementation Report
## Multilevel Feedback Queue (MLFQ) Scheduling Implementation

- **Date:** November 2, 2026
- **Commit Message:** `feat: implement multilevel feedback queue CPU scheduling`
- **Parent Commit:** `cbcb13f2f8fed93ae04549d719b2d8de2c77c1bc` (Batch 7)
- **Target Branch:** `main`

---

## 1. Executive Summary

Batch 8 implements a complete, robust, and mathematically verified **Multilevel Feedback Queue (MLFQ)** CPU scheduling algorithm for the modern C++20 CPU Scheduling Simulator.

MLFQ addresses the fundamental scheduling challenge of optimizing both turnaround time (by running shorter jobs first, similar to SJF) and response time (by running jobs interactively via Round Robin), without requiring prior knowledge of process burst times. Processes enter the highest-priority queue ($Q_0$) with a small time quantum. As a process consumes CPU time, it is systematically demoted to lower-priority queues with progressively longer time quanta, separating short/interactive jobs from compute-intensive long jobs. To prevent indefinite starvation of compute-bound processes in lower queues and to accommodate jobs that change behavior over time, a configurable periodic priority boost promotes all unfinished processes back to $Q_0$.

The implementation consists of:
- Public header: `include/cpu_scheduler/mlfq_scheduler.hpp` introducing `MlfqLevelConfig`, `MlfqConfig`, and `MlfqScheduler`.
- Implementation engine: `src/mlfq_scheduler.cpp` with discrete event-driven simulation, per-process level allotment tracking, non-resetting preemption semantics, priority boosting, and timeline normalization.
- Comprehensive test suite: `tests/test_mlfq_scheduler.cpp` adding **77 new GoogleTest unit tests** covering all 5 required categories and Scenarios A through J.
- Example application: `examples/basic_usage.cpp` demonstrating MLFQ side-by-side with FCFS, SJF, SRTF, RR, Priority, and MLQ.
- Documentation: Updated `README.md` and `docs/ARCHITECTURE.md`.
- Test results: **448/448 tests passing (100%)** across both Debug and Release configurations with zero compiler warnings under MSVC `/W4 /permissive-`.

---

## 2. Baseline Verification Details

Prior to making any modifications, the repository baseline was inspected:
- **Git Branch:** `main`
- **HEAD Commit:** `cbcb13f2f8fed93ae04549d719b2d8de2c77c1bc` (Batch 7: Multilevel Queue Scheduling)
- **Parent Commit:** `a0998f8ec48248aa87d75bac99e35aa27da93972` (Batch 6: Priority Scheduling)
- **Baseline Test Suite:** 371 GoogleTest unit tests across 8 test suites (`ProcessTest`, `ProcessValidatorTest`, `FcfsSchedulerTest`, `SjfSchedulerTest`, `SrtfSchedulerTest`, `RrSchedulerTest`, `PrioritySchedulerTest`, `MlqSchedulerTest`), all passing (100%).
- **Untracked Preserved Files:** `BATCH_5_INDEPENDENT_AUDIT_REPORT.md`, `BATCH_6_INDEPENDENT_AUDIT_REPORT.md`, `BATCH_7_INDEPENDENT_AUDIT_REPORT.md` (preserved historical audit reports, excluded from staging).

---

## 3. Public API Design and Justification

The public API is declared in `include/cpu_scheduler/mlfq_scheduler.hpp` under namespace `cpu_scheduler`:

### 3.1 `MlfqLevelConfig`
```cpp
struct MlfqLevelConfig {
    Time time_quantum{1};
    std::optional<Time> cpu_allotment{std::nullopt};

    constexpr explicit MlfqLevelConfig(Time quantum, std::optional<Time> allotment = std::nullopt) noexcept;
    [[nodiscard]] constexpr Time effective_allotment() const noexcept;
    [[nodiscard]] bool operator==(const MlfqLevelConfig& other) const = default;
};
```
- **Justification**: Separates the execution slice per turn (`time_quantum`) from the total CPU time allowed before demotion (`cpu_allotment`). When `allotment` is omitted (`std::nullopt`), `effective_allotment()` defaults to `time_quantum`, aligning with classic MLFQ where each quantum expiration demotes the process. When allotment is larger than quantum, a process rotates within the same queue before being demoted.

### 3.2 `MlfqConfig`
```cpp
struct MlfqConfig {
    std::vector<MlfqLevelConfig> levels{};
    std::optional<Time> boost_interval{std::nullopt};

    MlfqConfig() = default;
    explicit MlfqConfig(std::vector<MlfqLevelConfig> queue_levels, std::optional<Time> boost = std::nullopt);
    MlfqConfig(std::initializer_list<Time> quantums, std::optional<Time> boost = std::nullopt);

    [[nodiscard]] bool is_boost_enabled() const noexcept;
    MlfqConfig& add_level(Time time_quantum, std::optional<Time> allotment = std::nullopt);
    MlfqConfig& set_boost_interval(std::optional<Time> interval) noexcept;
    [[nodiscard]] bool operator==(const MlfqConfig& other) const = default;
};
```
- **Justification**: Provides fluid builder methods (`add_level`, `set_boost_interval`), initializer-list convenience constructor for uniform quantums, and explicit enablement checking for priority boosting (`is_boost_enabled()`).

### 3.3 `MlfqScheduler`
```cpp
class MlfqScheduler {
public:
    explicit MlfqScheduler(MlfqConfig config = default_config());

    [[nodiscard]] ScheduleOutcome schedule(std::span<const Process> workload) const;
    [[nodiscard]] ScheduleOutcome schedule(const std::vector<Process>& workload) const;

    [[nodiscard]] static ScheduleOutcome run(std::span<const Process> workload,
                                             const MlfqConfig& config = default_config());
    [[nodiscard]] static ScheduleOutcome run(const std::vector<Process>& workload,
                                             const MlfqConfig& config = default_config());

    [[nodiscard]] const MlfqConfig& config() const noexcept;
    [[nodiscard]] static MlfqConfig default_config();
};
```
- **Justification**: Mirrors the established architecture of `FcfsScheduler`, `SjfScheduler`, `SrtfScheduler`, `RrScheduler`, and `MlqScheduler`. Supports both instance-based configuration and static one-shot invocation (`run`), accepting both `std::span<const Process>` and `const std::vector<Process>&`.

---

## 4. MLFQ State Model and Queue-Level Lifecycle

### 4.1 Internal Process State (`MlfqProcessEntry`)
To track dynamic lifecycle variables during MLFQ simulation without mutating the original `Process`, each process is mapped to an internal entry:
- `process_id`: Identifier string.
- `arrival_time`: Workload arrival timestamp.
- `burst_time`: Workload total CPU burst.
- `priority`: Workload priority integer.
- `original_index`: Index within the input sequence for deterministic tie-breaking.
- `remaining_burst`: CPU burst time remaining to execute.
- `current_level`: Current queue level index ($0 \le \text{level} < L$).
- `current_level_allotment_remaining`: CPU time remaining before demotion at the current level.
- `current_quantum_remaining`: CPU time remaining in the current Round Robin turn.
- `first_start_time`: Optional timestamp when the process first received CPU execution.
- `completion_time`: Timestamp when `remaining_burst == 0`.

### 4.2 Queue-Level Hierarchy and Lifecycle
- **Queue Count**: Configured with $L \ge 2$ levels. Queue 0 has highest priority; Queue $L-1$ has lowest priority.
- **Initial Placement**: Every newly arriving process unconditionally enters Queue 0.
- **Dispatch**: At each scheduling step, the scheduler selects the head of the lowest-indexed non-empty queue ($Q_0, Q_1, \dots$).
- **Completion**: When a process reaches `remaining_burst == 0`, it is marked completed, its completion metrics (`TAT = CT - AT`, `WT = TAT - BT`, `RT = ST - AT`) are finalized, and it is permanently removed from the ready queues.

---

## 5. CPU Allotment Tracking Mechanism Across Preemptions

In robust MLFQ implementations (preventing gaming of the scheduler), a process must not reset its allotment when it relinquishes the CPU or is preempted by a higher-priority arrival:
1. When a process runs, its `remaining_burst`, `current_quantum_remaining`, and `current_level_allotment_remaining` decrease by the elapsed slice duration $\Delta t$.
2. **Preemption by Higher-Priority Arrival**: When a new process arrives at Queue 0 while a process $P$ is executing at level $l > 0$, $P$ is preempted immediately at the arrival timestamp.
3. **Preservation**: The preempted process $P$ retains its accumulated `current_level_allotment_remaining` and `current_quantum_remaining`.
4. **Queue Positioning**: $P$ is placed at the **front** of Queue $l$ (`push_front`). When higher-priority queues become empty again, $P$ immediately resumes its turn without having lost its remaining allotment or quantum.
5. **No Gaming**: A process cannot repeatedly run for most of its quantum, get preempted by a newly arrived job, and receive a refreshed allotment.

---

## 6. Demotion Policy and Bottom-Queue Behavior

### 6.1 Demotion
- When a process exhausts its level allotment (`current_level_allotment_remaining == 0`):
  - If `current_level < num_levels - 1`: The process is demoted to `current_level + 1`.
  - Its allotment and quantum are refreshed to the parameters of the new level:
    - `current_level_allotment_remaining = levels[current_level].effective_allotment()`
    - `current_quantum_remaining = levels[current_level].time_quantum`
  - It is appended to the **tail** of the next lower queue (`push_back`).

### 6.2 Quantum Expiration Without Allotment Expiration
- If `current_quantum_remaining == 0` but `current_level_allotment_remaining > 0`:
  - The process stays at `current_level`.
  - Its quantum is refreshed (`current_quantum_remaining = levels[current_level].time_quantum`).
  - It rotates to the **tail** of `current_level` (`push_back`), allowing other ready processes at the same level to execute.

### 6.3 Bottom-Level Queue Behavior
- If `current_level == num_levels - 1` (the lowest queue):
  - A process cannot be demoted further.
  - Upon exhausting its allotment or quantum, it remains at `current_level`.
  - Its allotment and quantum are refreshed, and it is appended to the back of the bottom queue (`push_back`).

---

## 7. Preemption Mechanism Between Queue Levels

1. **Preemption Rule**: A process executing at queue level $l$ is preempted **if and only if** a process arrives in a strictly higher-priority queue level $k < l$.
2. **Equal-Level Non-Preemption**: An incoming arrival at the *same* level $l$ (which occurs when running at Queue 0 and another job arrives at Queue 0) does **not** preempt the currently executing process. The arriving process joins the tail of Queue 0, and the running process completes its quantum or burst.
3. **Discrete Event Advancement**: When running a process at level $l > 0$, the engine calculates the maximum safe execution duration:
   $$\Delta t = \min(\text{remaining\_burst}, \text{current\_quantum}, \text{current\_allotment}, \text{next\_higher\_level\_arrival} - \text{current\_time}, \text{next\_boost} - \text{current\_time})$$
   This guarantees that preemption occurs at the exact timestamp of arrival without polling.

---

## 8. Priority Boosting Mechanism and Starvation Prevention

### 8.1 Periodic Priority Boost
- To eliminate starvation of CPU-bound processes in lower queues and to accommodate jobs that shift from CPU-bound to interactive, a global periodic boost occurs at intervals of `boost_interval` ($t = k \times \text{boost\_interval}$ for $k = 1, 2, \dots$).
- When `boost_interval` is omitted or set to $0$, boosting is disabled.

### 8.2 Interruption and Timeline Closure
- If a process is executing when the boost timestamp is reached, its execution is interrupted at exactly $t_{\text{boost}}$.
- The active timeline segment $[t_{\text{start}}, t_{\text{boost}}]$ is closed.

### 8.3 Reset and Deterministic Tie-Breaking
- All uncompleted processes across all queues (and any process currently executing) are collected and promoted to Queue 0:
  - `current_level = 0`
  - `current_level_allotment_remaining = levels[0].effective_allotment()`
  - `current_quantum_remaining = levels[0].time_quantum`
- All lower queues ($Q_1 \dots Q_{L-1}$) are cleared.
- Promoted processes are placed in Queue 0 sorted deterministically by:
  1. **Primary**: Earlier arrival time (`arrival_time`).
  2. **Secondary**: Earlier workload input index (`original_index`).

---

## 9. Event Ordering at Simultaneous Timestamps

When multiple scheduling events coincide at the exact same simulation timestamp $t$, events are processed in strict priority order:
1. **Periodic Priority Boost**: Boost fires first, promoting all unfinished processes to Queue 0 and resetting their parameters.
2. **Workload Arrivals**: Newly arrived processes at timestamp $t$ enter Queue 0 (appended in original workload input order).
3. **Queue Selection**: The scheduler dispatches the process at the head of Queue 0.
4. **Timeline Merging**: If the newly dispatched process is identical to the process that was executing immediately prior to the event (and no other process intervened), consecutive segments are normalized into a single continuous timeline block.

---

## 10. Scenarios A Through J: Manual Traces & Simulation Results

### Scenario A: Standard 3-Level MLFQ Demotion of Single Long Process
- **Configuration**: $Q_0(q=2), Q_1(q=4), Q_2(q=8)$. Boost disabled.
- **Workload**: $P_1(AT=0, BT=10)$.
- **Independent Manual Trace**:
  - $t=0$: $P_1$ arrives at $Q_0$. Runs for quantum 2 $\rightarrow [0, 2]$. Allotment exhausted $\rightarrow$ demoted to $Q_1$ with remaining burst 8.
  - $t=2$: $P_1$ runs in $Q_1$ for quantum 4 $\rightarrow [2, 6]$. Allotment exhausted $\rightarrow$ demoted to $Q_2$ with remaining burst 4.
  - $t=6$: $P_1$ runs in $Q_2$ for burst 4 $\rightarrow [6, 10]$. Completes at $t=10$.
- **Metrics**: $CT=10, TAT=10, WT=0, RT=0$.
- **Simulation Match**: Verified by `MlfqDemotionTest.ScenarioA_SingleLongProcessDemotedThroughAllLevels`. Exact match.

---

### Scenario B: Short Interactive Job vs. Long Compute Job
- **Configuration**: $Q_0(q=2), Q_1(q=4), Q_2(q=8)$. Boost disabled.
- **Workload**: $P_1(AT=0, BT=10), P_2(AT=1, BT=2)$.
- **Independent Manual Trace**:
  - $t=0$: $P_1$ starts in $Q_0$.
  - $t=1$: $P_2$ arrives in $Q_0$. Since $P_1$ is at $Q_0$ (equal level), $P_1$ is not preempted and runs until its quantum expires at $t=2$.
  - $t=2$: $P_1$ exhausts $Q_0$ quantum $\rightarrow$ demoted to $Q_1$ (remaining burst 8). $P_2$ is at head of $Q_0$.
  - $t=2..4$: $P_2$ executes in $Q_0$ for 2 units $\rightarrow$ completes at $t=4$.
  - $t=4..8$: $P_1$ executes in $Q_1$ for 4 units $\rightarrow$ demoted to $Q_2$ (remaining burst 4).
  - $t=8..12$: $P_1$ executes in $Q_2$ for 4 units $\rightarrow$ completes at $t=12$.
- **Metrics**:
  - $P_1$: $ST=0, CT=12, TAT=12, WT=2, RT=0$.
  - $P_2$: $ST=2, CT=4, TAT=3, WT=1, RT=1$.
- **Simulation Match**: Verified by `MlfqDemotionTest.ScenarioB_TwoProcessesOneShortOneLong`. Exact match.

---

### Scenario C: Higher-Priority Preemption by Late Arrival
- **Configuration**: $Q_0(q=2), Q_1(q=4), Q_2(q=8)$. Boost disabled.
- **Workload**: $P_1(AT=0, BT=10), P_2(AT=3, BT=1)$.
- **Independent Manual Trace**:
  - $t=0..2$: $P_1$ runs in $Q_0$ $\rightarrow$ demoted to $Q_1$ (remaining burst 8).
  - $t=2..3$: $P_1$ runs in $Q_1$ for 1 unit (remaining burst 7, remaining $Q_1$ allotment 3).
  - $t=3$: $P_2$ arrives in $Q_0$ (higher priority than $Q_1$). $P_1$ is preempted immediately.
  - $t=3..4$: $P_2$ runs in $Q_0$ for 1 unit $\rightarrow$ completes at $t=4$.
  - $t=4..7$: $P_1$ resumes in $Q_1$ at head of queue for its remaining 3 units of allotment $\rightarrow$ demoted to $Q_2$ (remaining burst 4).
  - $t=7..11$: $P_1$ runs in $Q_2$ for 4 units $\rightarrow$ completes at $t=11$.
- **Metrics**:
  - $P_1$: $ST=0, CT=11, TAT=11, WT=1, RT=0$.
  - $P_2$: $ST=3, CT=4, TAT=1, WT=0, RT=0$.
- **Simulation Match**: Verified by `MlfqPreemptionTest.ScenarioC_HigherPriorityPreemptionByLateArrival`. Exact match.

---

### Scenario D: Preempted Process Preserves Remaining Allotment
- **Configuration**: $Q_0(q=2), Q_1(q=4), Q_2(q=8)$. Boost disabled.
- **Workload**: $P_1(AT=0, BT=10), P_2(AT=3, BT=2)$.
- **Independent Manual Trace**:
  - $t=0..2$: $P_1$ runs in $Q_0$ $\rightarrow$ demoted to $Q_1$.
  - $t=2..3$: $P_1$ runs in $Q_1$ for 1 unit. Allotment remaining: 3.
  - $t=3$: $P_2$ arrives in $Q_0$ $\rightarrow$ preempts $P_1$.
  - $t=3..5$: $P_2$ runs in $Q_0$ for 2 units $\rightarrow$ demoted to $Q_1$ (remaining burst 0? No, burst is 2 so it completes at $t=5$).
  - $t=5..8$: $P_1$ resumes in $Q_1$ for its remaining 3 units $\rightarrow$ demoted to $Q_2$ at $t=8$ (remaining burst 4).
  - $t=8..12$: $P_1$ runs in $Q_2$ for 4 units $\rightarrow$ completes at $t=12$.
- **Metrics**:
  - $P_1$: $CT=12, TAT=12, WT=2$.
  - $P_2$: $CT=5, TAT=2, WT=0, RT=0$.
- **Simulation Match**: Verified by `MlfqPreemptionTest.ScenarioD_PreemptedProcessPreservesRemainingAllotment`. Exact match.

---

### Scenario E: Periodic Priority Boost Promotes to $Q_0$
- **Configuration**: $Q_0(q=2), Q_1(q=4), Q_2(q=8)$. Boost interval = 10.
- **Workload**: $P_1(AT=0, BT=12)$.
- **Independent Manual Trace**:
  - $t=0..2$: $P_1$ runs in $Q_0$ $\rightarrow$ demoted to $Q_1$ (rem=10).
  - $t=2..6$: $P_1$ runs in $Q_1$ $\rightarrow$ demoted to $Q_2$ (rem=6).
  - $t=6..10$: $P_1$ runs in $Q_2$ for 4 units (rem=2).
  - $t=10$: Boost fires! $P_1$ is promoted to $Q_0$ with fresh quantum 2 and allotment 2.
  - $t=10..12$: $P_1$ runs in $Q_0$ for 2 units $\rightarrow$ completes at $t=12$.
- **Metrics**: $CT=12, TAT=12, WT=0, RT=0$.
- **Simulation Match**: Verified by `MlfqBoostingTest.ScenarioE_PeriodicPriorityBoostPromotesToQ0`. Exact match.

---

### Scenario F: Boost Interrupts Executing Process at Exact Boost Timestamp
- **Configuration**: $Q_0(q=2), Q_1(q=4), Q_2(q=8)$. Boost interval = 5.
- **Workload**: $P_1(AT=0, BT=10)$.
- **Independent Manual Trace**:
  - $t=0..2$: $P_1$ runs in $Q_0$ $\rightarrow$ demoted to $Q_1$.
  - $t=2..5$: $P_1$ runs in $Q_1$ for 3 units (rem=5). At $t=5$, boost interrupts $P_1$.
  - $t=5$: $P_1$ promoted to $Q_0$ with fresh parameters.
  - $t=5..7$: $P_1$ runs in $Q_0$ for 2 units $\rightarrow$ demoted to $Q_1$ (rem=3).
  - $t=7..10$: $P_1$ runs in $Q_1$ for 3 units $\rightarrow$ completes at $t=10$ (exactly when 2nd boost occurs).
- **Metrics**: $CT=10, TAT=10, WT=0, RT=0$. Timeline: $[0, 2], [2, 5], [5, 7], [7, 10]$ (normalized to continuous $[0, 10]$).
- **Simulation Match**: Verified by `MlfqBoostingTest.ScenarioF_BoostInterruptsExecutingProcessAtExactTimestamp`. Exact match.

---

### Scenario G: New Process Arrives at Exact Boost Timestamp
- **Configuration**: $Q_0(q=2), Q_1(q=4), Q_2(q=8)$. Boost interval = 10.
- **Workload**: $P_1(AT=0, BT=14), P_2(AT=10, BT=2)$.
- **Independent Manual Trace**:
  - $t=0..2$: $P_1$ runs in $Q_0$ $\rightarrow$ demoted to $Q_1$.
  - $t=2..6$: $P_1$ runs in $Q_1$ $\rightarrow$ demoted to $Q_2$.
  - $t=6..10$: $P_1$ runs in $Q_2$ for 4 units (rem=4).
  - $t=10$: Boost promotes $P_1$ to $Q_0$. $P_2$ arrives at $t=10$ in $Q_0$.
  - Tie-breaking: $P_1$ has $AT=0 < P_2$ ($AT=10$). $P_1$ executes first!
  - $t=10..12$: $P_1$ executes in $Q_0$ for 2 units $\rightarrow$ demoted to $Q_1$ (rem=2).
  - $t=12..14$: $P_2$ executes in $Q_0$ for 2 units $\rightarrow$ completes at $t=14$.
  - $t=14..16$: $P_1$ executes in $Q_1$ for 2 units $\rightarrow$ completes at $t=16$.
- **Metrics**:
  - $P_1$: $CT=16, TAT=16, WT=2$.
  - $P_2$: $CT=14, TAT=4, WT=2, RT=2$.
- **Simulation Match**: Verified by `MlfqBoostingTest.ScenarioG_ArrivalExactlyAtBoostTimestamp`. Exact match.

---

### Scenario H: Simultaneous Arrivals at $Q_0$ Ordered by Input Sequence
- **Configuration**: $Q_0(q=2), Q_1(q=4)$. Boost disabled.
- **Workload**: $P_1(AT=0, BT=3), P_2(AT=0, BT=3)$.
- **Independent Manual Trace**:
  - Both arrive at $t=0$ in $Q_0$. $P_1$ appears before $P_2$ in input order.
  - $t=0..2$: $P_1$ runs in $Q_0$ for 2 units $\rightarrow$ demoted to $Q_1$ (rem=1).
  - $t=2..4$: $P_2$ runs in $Q_0$ for 2 units $\rightarrow$ demoted to $Q_1$ (rem=1).
  - In $Q_1$, $P_1$ was enqueued before $P_2$.
  - $t=4..5$: $P_1$ runs in $Q_1$ for 1 unit $\rightarrow$ completes at $t=5$.
  - $t=5..6$: $P_2$ runs in $Q_1$ for 1 unit $\rightarrow$ completes at $t=6$.
- **Metrics**:
  - $P_1$: $CT=5, TAT=5, WT=2, RT=0$.
  - $P_2$: $CT=6, TAT=6, WT=3, RT=2$.
- **Simulation Match**: Verified by `MlfqValidationTest.ScenarioH_SimultaneousArrivalsAtQ0OrderedByInputSequence`. Exact match.

---

### Scenario I: Allotment Larger Than Quantum Causes Rotation Within Level
- **Configuration**: $Q_0(q=2, \text{allotment}=4), Q_1(q=4, \text{allotment}=8)$.
- **Workload**: $P_1(AT=0, BT=6), P_2(AT=0, BT=6)$.
- **Independent Manual Trace**:
  - Both arrive at $t=0$ in $Q_0$.
  - $t=0..2$: $P_1$ runs 2 units. Quantum expires, but allotment remaining is $4-2=2 > 0$. $P_1$ rotates to tail of $Q_0$.
  - $t=2..4$: $P_2$ runs 2 units. Quantum expires, allotment remaining is 2. $P_2$ rotates to tail of $Q_0$.
  - $t=4..6$: $P_1$ runs 2 units. Allotment exhausted ($4-4=0$) $\rightarrow$ demoted to $Q_1$ (rem=2).
  - $t=6..8$: $P_2$ runs 2 units. Allotment exhausted $\rightarrow$ demoted to $Q_1$ (rem=2).
  - $t=8..10$: $P_1$ runs in $Q_1$ for 2 units $\rightarrow$ completes at $t=10$.
  - $t=10..12$: $P_2$ runs in $Q_1$ for 2 units $\rightarrow$ completes at $t=12$.
- **Metrics**:
  - $P_1$: $CT=10, TAT=10, WT=4, RT=0$.
  - $P_2$: $CT=12, TAT=12, WT=6, RT=2$.
- **Simulation Match**: Verified by `MlfqDemotionTest.ScenarioI_AllotmentLargerThanQuantumCausesRotationBeforeDemotion`. Exact match.

---

### Scenario J: Multiple Processes at Different Levels with Arrivals and Promotions
- **Configuration**: $Q_0(q=2), Q_1(q=4), Q_2(q=8)$. Boost interval = 15.
- **Workload**: $P_1(AT=0, BT=12), P_2(AT=3, BT=4), P_3(AT=8, BT=2)$.
- **Independent Manual Trace**:
  - $t=0..2$: $P_1$ runs in $Q_0$ $\rightarrow$ demoted to $Q_1$ (rem=10).
  - $t=2..3$: $P_1$ runs in $Q_1$ for 1 unit (rem=9, all=3).
  - $t=3$: $P_2$ arrives in $Q_0$ $\rightarrow$ preempts $P_1$.
  - $t=3..5$: $P_2$ runs in $Q_0$ for 2 units $\rightarrow$ demoted to $Q_1$ (rem=2).
  - $t=5$: $Q_0$ empty. In $Q_1$, $P_1$ is at head, $P_2$ is at tail. $P_1$ runs for its remaining 3 units of allotment $\rightarrow [5, 8]$.
  - $t=8$: $P_1$ demoted to $Q_2$ (rem=6). Simultaneously, $P_3$ arrives at $Q_0$!
  - $t=8..10$: $P_3$ runs in $Q_0$ for 2 units $\rightarrow$ completes at $t=10$.
  - $t=10$: $Q_0$ empty. In $Q_1$, $P_2$ is waiting with rem=2. In $Q_2$, $P_1$ is waiting with rem=6. $Q_1$ has priority over $Q_2$.
  - $t=10..12$: $P_2$ runs in $Q_1$ for 2 units $\rightarrow$ completes at $t=12$.
  - $t=12..15$: $P_1$ runs in $Q_2$ for 3 units (rem=3).
  - $t=15$: Boost fires! $P_1$ promoted to $Q_0$ with fresh quantum 2 and allotment 2.
  - $t=15..17$: $P_1$ runs in $Q_0$ for 2 units $\rightarrow$ demoted to $Q_1$ (rem=1).
  - $t=17..18$: $P_1$ runs in $Q_1$ for 1 unit $\rightarrow$ completes at $t=18$.
- **Metrics**:
  - $P_1$: $ST=0, CT=18, TAT=18, WT=6, RT=0$.
  - $P_2$: $ST=3, CT=12, TAT=9, WT=5, RT=0$.
  - $P_3$: $ST=8, CT=10, TAT=2, WT=0, RT=0$.
- **Simulation Match**: Verified by `MlfqEventTest.ScenarioJ_MultipleLowerLevelProcesses`. Exact match.

---

## 11. Comprehensive Unit Test Coverage Matrix

A total of **77 new GoogleTest unit tests** were authored in `tests/test_mlfq_scheduler.cpp`, organized across 5 distinct test fixtures:

| Test Fixture | Test Count | Key Areas Covered |
| :--- | :---: | :--- |
| **`MlfqValidationTest`** | 15 | Minimum level count ($\ge 2$), positive quantum validation, positive allotment validation, negative/zero boost interval validation, empty workload rejection, invalid process attributes, simultaneous arrival order (Scenario H), boundary values. |
| **`MlfqDemotionTest`** | 15 | Single long process across levels (Scenario A), two processes short/long (Scenario B), multi-step demotion, bottom-level stay behavior, queue exhaustion, allotment larger than quantum (Scenario I), process completion without demotion. |
| **`MlfqPreemptionTest`** | 16 | Higher-priority preemption (Scenario C), allotment preservation across preemption (Scenario D), preemption across multiple lower levels, equal-level non-preemption in $Q_0$, turnaround/waiting time relationship invariants, first start time immutability across preemptions. |
| **`MlfqBoostingTest`** | 16 | Boost promotion to $Q_0$ (Scenario E), boost interruption at exact timestamp (Scenario F), simultaneous arrival and boost (Scenario G), multiple consecutive periodic boosts, boost during CPU idle, completed process exclusion, clearing of lower queues, deterministic tie-breaking. |
| **`MlfqEventTest`** | 15 | Complex multi-process interaction (Scenario J), 5 processes simultaneous arrival, simultaneous completion and arrival, vector vs span parity, instance vs static parity, timeline chronologicity, workload immutability, repeated deterministic runs, idle fast-forwarding. |
| **Total Batch 8 Tests** | **77** | **All 77 passed with 100% success rate.** |

---

## 12. Debug and Release Build and Test Results

### 12.1 Debug Build & CTest
- **Configuration Preset:** `windows-msvc-debug`
- **Compiler Flags:** `/std:c++20 /W4 /permissive-`
- **Build Outcome:** Succeeded with **0 warnings and 0 errors**.
- **CTest Command:** `ctest --preset debug-test --output-on-failure`
- **CTest Results:**
  ```text
  100% tests passed out of 448
  Total Test time (real) = 19.36 sec
  ```

### 12.2 Release Build & CTest
- **Configuration Preset:** `windows-msvc-release`
- **Compiler Flags:** `/std:c++20 /O2 /W4 /permissive-`
- **Build Outcome:** Succeeded with **0 warnings and 0 errors**.
- **CTest Command:** `ctest --preset release-test --output-on-failure`
- **CTest Results:**
  ```text
  100% tests passed out of 448
  Total Test time (real) = 12.07 sec
  ```

---

## 13. Arithmetic Overflow Safety Strategy

All time-related accumulations and interval arithmetic throughout `src/mlfq_scheduler.cpp` strictly avoid unchecked addition:
1. **`safe_add` Helper**: Checked addition using standard limits:
   ```cpp
   [[nodiscard]] constexpr std::optional<Time> safe_add(Time a, Time b) noexcept {
       if (b > 0 && a > std::numeric_limits<Time>::max() - b) return std::nullopt;
       if (b < 0 && a < std::numeric_limits<Time>::min() - b) return std::nullopt;
       return a + b;
   }
   ```
2. **Current Time Advancement**: `current_time` is advanced via `safe_add(current_time, slice)`. If overflow is detected, `ScheduleErrorKind::ArithmeticOverflow` is returned immediately.
3. **Metric Accumulation**: Total waiting time, turnaround time, busy time, and idle time computations in summary metric generation use checked addition.
4. **Boost Timestamp Calculation**: Future boost timestamps `next_boost` use safe addition.

---

## 14. Algorithmic Time and Space Complexity

Let:
- $N$ = total number of processes in the workload.
- $B$ = sum of all process burst times ($\sum BT$).
- $q_{\min} = \min_{l} (\text{levels}[l].\text{time\_quantum})$.
- $K_{\text{boost}}$ = number of priority boost events ($B / \text{boost\_interval}$).

### Time Complexity
1. **Validation & Initial Sorting**: $O(N \log N)$ to validate and sort arriving processes by arrival timestamp.
2. **Execution Steps**: Each slice executes for at least $\min(q_{\min}, \text{remaining\_burst})$. The number of execution slices is bounded by $O(B / q_{\min} + N)$.
3. **Preemptions**: A process running at level $l > 0$ can be preempted at most once per higher-priority arrival ($O(N)$ preemptions across the entire run).
4. **Boosting**: Each boost sorts remaining uncompleted processes deterministically in $O(N \log N)$. With $K_{\text{boost}}$ boosts, this adds $O(K_{\text{boost}} \cdot N \log N)$.
5. **Total Time Complexity**:
   $$\mathcal{O}\left(N \log N + \frac{B}{q_{\min}} + K_{\text{boost}} \cdot N \log N\right)$$

### Space Complexity
1. **Process Tracking**: $O(N)$ auxiliary space for `MlfqProcessEntry` records and queue nodes across all levels.
2. **Timeline Segments**: At most $O(N + B / q_{\min})$ segments in the execution history.
3. **Total Space Complexity**: $\mathcal{O}\left(N + \frac{B}{q_{\min}}\right)$.

---

## 15. Documentation and Example Updates

- **`README.md`**:
  - Updated status to Batch 8.
  - Added Section 8 detailing MLFQ philosophy, rules, allotment tracking, and priority boosting.
  - Updated test counts to 448 tests in instructions.
  - Updated project directory tree to include Batch 8 files.
- **`docs/ARCHITECTURE.md`**:
  - Updated architectural layer diagram to include `MlfqScheduler`.
  - Added Section 2.12 detailing MLFQ component boundaries, queue lifecycles, and demotion mechanics.
  - Updated Section 3 for future extensions (Batches 9+).
- **`examples/basic_usage.cpp`**:
  - Added `#include "cpu_scheduler/mlfq_scheduler.hpp"`.
  - Added Section 9 demonstrating 3-tier MLFQ with preemption and periodic boosting.

---

## 16. Git Commit and Repository Status

- **Branch:** `main`
- **Target Commit Message:** `feat: implement multilevel feedback queue CPU scheduling`
- **Working Tree Files To Stage:**
  - `CMakeLists.txt`
  - `tests/CMakeLists.txt`
  - `include/cpu_scheduler/mlfq_scheduler.hpp`
  - `src/mlfq_scheduler.cpp`
  - `tests/test_mlfq_scheduler.cpp`
  - `examples/basic_usage.cpp`
  - `README.md`
  - `docs/ARCHITECTURE.md`
  - `BATCH_8_IMPLEMENTATION_REPORT.md`
- **Historical Untracked Files Retained:**
  - `BATCH_5_INDEPENDENT_AUDIT_REPORT.md`
  - `BATCH_6_INDEPENDENT_AUDIT_REPORT.md`
  - `BATCH_7_INDEPENDENT_AUDIT_REPORT.md`

---

## 17. Known Limitations and Scope Exclusions

- **I/O Bursts & Sleep States**: The simulation model considers CPU bursts only; processes do not perform simulated disk or network I/O operations.
- **Dynamic Queue Level Count**: The number of queues and their parameters are configured at initialization and remain fixed during simulation.
- **Command-Line Interface**: Interactive CLI and file-based workload parsing are intentionally reserved for Batch 9.

---

## 18. Final Sign-Off

The Batch 8 Multilevel Feedback Queue (MLFQ) scheduling engine is complete, fully tested, documented, and verified. All 448 unit tests pass with zero warnings in both Debug and Release presets. The implementation is ready for independent verification audit.
