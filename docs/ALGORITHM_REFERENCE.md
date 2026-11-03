# CPU Scheduling Simulator — Algorithm Reference

A comprehensive theoretical and practical specification of the eight CPU scheduling algorithms implemented in the C++20 CPU Scheduling Simulator.

---

## Table of Contents

1. [Common Concepts & Invariants](#1-common-concepts--invariants)
2. [First Come, First Served (FCFS)](#2-first-come-first-served-fcfs)
3. [Shortest Job First (SJF, Non-Preemptive)](#3-shortest-job-first-sjf-non-preemptive)
4. [Shortest Remaining Time First (SRTF, Preemptive)](#4-shortest-remaining-time-first-srtf-preemptive)
5. [Round Robin (RR)](#5-round-robin-rr)
6. [Non-Preemptive Priority Scheduling](#6-non-preemptive-priority-scheduling)
7. [Preemptive Priority Scheduling](#7-preemptive-priority-scheduling)
8. [Multilevel Queue (MLQ)](#8-multilevel-queue-mlq)
9. [Multilevel Feedback Queue (MLFQ)](#9-multilevel-feedback-queue-mlfq)
10. [Algorithm Comparison Matrix](#10-algorithm-comparison-matrix)

---

## 1. Common Concepts & Invariants

Across all scheduling algorithms in the simulator, standard definitions apply:

- **Simulation Time ($t$)**: Represented as a 64-bit integer (`Time = std::int64_t`), starting at $t=0$.
- **Arrival Time ($AT$)**: Timestamp when a process becomes ready for CPU execution.
- **Burst Time ($BT$)**: Total CPU execution time required by the process.
- **Completion Time ($CT$)**: Timestamp when the process finishes its final CPU burst.
- **Turnaround Time ($TAT$)**: Total elapsed time from arrival to completion:
  $$\text{Turnaround Time} = CT - AT$$
- **Waiting Time ($WT$)**: Total time a process spent waiting in ready queues without CPU execution:
  $$\text{Waiting Time} = TAT - BT$$
- **Response Time ($RT$)**: Elapsed time from arrival until the process first receives CPU execution:
  $$\text{Response Time} = \text{First Start Time} - AT$$
- **Priority Direction**: In all priority-aware algorithms, **smaller numerical values represent higher scheduling priority**. For example, priority `0` is higher than priority `2`, and priority `-1` is higher than `0`.
- **CPU Idle Periods**: When no processes are ready to run, the CPU enters an explicit idle state, represented in the timeline by an idle segment. Process execution resumes immediately upon the next arrival.
- **Timeline Contiguity**: Timelines are strictly contiguous from $t=0$ to completion without unrecorded gaps or overlapping executions.

---

## 2. First Come, First Served (FCFS)

### 2.1 Principle & Philosophy
First Come, First Served is the simplest scheduling discipline. It dispatches ready processes in the strict order of their arrival into the system.

- **Preemption**: **Non-preemptive**. Once a process receives the CPU, it runs uninterrupted until its entire burst time is satisfied.
- **Queue Structure**: Single First-In, First-Out (FIFO) queue.

### 2.2 Selection & Tie-Breaking Rules
1. **Primary**: Earliest arrival time ($AT$).
2. **Tie-Breaker**: Stable preserving of original input sequence order for simultaneous arrivals.

### 2.3 Configuration Options
- None.

### 2.4 Worked Example
**Workload**:
| Process | Arrival Time | Burst Time |
| :---: | :---: | :---: |
| **P1** | 0 | 5 |
| **P2** | 2 | 3 |
| **P3** | 4 | 2 |

**Execution Trace**:
- $t=0$: P1 arrives and starts running immediately.
- $t=2$: P2 arrives, waits behind P1.
- $t=4$: P3 arrives, waits behind P2.
- $t=5$: P1 completes ($CT=5$). P2 starts running.
- $t=8$: P2 completes ($CT=8$). P3 starts running.
- $t=10$: P3 completes ($CT=10$).

**Gantt Chart**:
```text
+-------+-----+---+
|  P1   | P2  |P3 |
+-------+-----+---+
0       5     8   10
```

**Result Metrics**:
- P1: $CT=5$, $TAT=5-0=5$, $WT=5-5=0$, $RT=0-0=0$
- P2: $CT=8$, $TAT=8-2=6$, $WT=6-3=3$, $RT=5-2=3$
- P3: $CT=10$, $TAT=10-4=6$, $WT=6-2=4$, $RT=8-4=4$
- **Averages**: $TAT_{\text{avg}} = 5.67$, $WT_{\text{avg}} = 2.33$, $RT_{\text{avg}} = 2.33$

### 2.5 Advantages & Limitations
- **Advantages**: Simple to understand and implement; zero scheduler overhead; starvation-free (every process eventually runs).
- **Limitations**: **Convoy Effect** — short processes arriving behind a long CPU-bound process suffer long waiting times, degrading average turnaround time.

---

## 3. Shortest Job First (SJF, Non-Preemptive)

### 3.1 Principle & Philosophy
Shortest Job First schedules the ready process with the smallest CPU burst time.

- **Preemption**: **Non-preemptive**. Once a process is dispatched, it runs to completion even if a shorter process arrives later.
- **Dynamic Ready-Queue Filtering**: Only processes with $AT \le t$ are eligible for selection. The scheduler does *not* sort the entire workload by burst time globally; it dynamically evaluates candidates currently waiting.

### 3.2 Selection & Tie-Breaking Rules
1. **Primary**: Smallest burst time ($BT$).
2. **Secondary**: Earlier arrival time ($AT$).
3. **Tertiary**: Stable input sequence order.

### 3.3 Configuration Options
- None.

### 3.4 Worked Example
**Workload**:
| Process | Arrival Time | Burst Time |
| :---: | :---: | :---: |
| **P1** | 0 | 7 |
| **P2** | 2 | 4 |
| **P3** | 4 | 1 |
| **P4** | 5 | 4 |

**Execution Trace**:
- $t=0$: Only P1 is ready ($BT=7$). P1 starts running.
- $t=2, 4, 5$: P2, P3, and P4 arrive, but P1 is non-preemptive and continues running.
- $t=7$: P1 completes ($CT=7$). Ready queue candidates: P2 ($BT=4$), P3 ($BT=1$), P4 ($BT=4$). Shortest is P3. P3 starts running.
- $t=8$: P3 completes ($CT=8$). Ready candidates: P2 ($BT=4$), P4 ($BT=4$). Tie on burst broken by earlier arrival: P2 ($AT=2$) before P4 ($AT=5$). P2 runs.
- $t=12$: P2 completes ($CT=12$). P4 runs.
- $t=16$: P4 completes ($CT=16$).

**Gantt Chart**:
```text
+---------+---+-----+-----+
|   P1    |P3 | P2  | P4  |
+---------+---+-----+-----+
0         7   8     12    16
```

**Result Metrics**:
- P1: $CT=7$, $TAT=7$, $WT=0$, $RT=0$
- P2: $CT=12$, $TAT=10$, $WT=6$, $RT=6$
- P3: $CT=8$, $TAT=4$, $WT=3$, $RT=3$
- P4: $CT=16$, $TAT=11$, $WT=7$, $RT=7$
- **Averages**: $TAT_{\text{avg}} = 8.00$, $WT_{\text{avg}} = 4.00$, $RT_{\text{avg}} = 4.00$

### 3.5 Advantages & Limitations
- **Advantages**: Provably minimizes average waiting time among non-preemptive algorithms when all jobs are ready simultaneously.
- **Limitations**: Long processes can suffer starvation if short processes arrive continuously.

---

## 4. Shortest Remaining Time First (SRTF, Preemptive)

### 4.1 Principle & Philosophy
Shortest Remaining Time First is the preemptive variant of SJF. At every point in time, the CPU is allocated to the process with the smallest remaining burst time.

- **Preemption**: **Preemptive**. When a process arrives with a remaining burst strictly shorter than the currently running process's remaining burst, the running process is immediately preempted and returned to the ready queue.
- **Strict Preemption Rule**: Competing process must have **strictly smaller** remaining time (`competing.remaining < running.remaining`). Equal remaining times do not cause preemption, avoiding unnecessary context switching overhead.

### 4.2 Selection & Tie-Breaking Rules
1. **Primary**: Smallest remaining burst time.
2. **Secondary**: Earlier arrival time.
3. **Tertiary**: Original input sequence order.

### 4.3 Configuration Options
- None.

### 4.4 Worked Example
**Workload**:
| Process | Arrival Time | Burst Time |
| :---: | :---: | :---: |
| **P1** | 0 | 8 |
| **P2** | 1 | 4 |
| **P3** | 2 | 2 |
| **P4** | 3 | 1 |

**Execution Trace**:
- $t=0$: P1 starts running (remaining: 8).
- $t=1$: P2 arrives with burst 4. P1 has remaining burst 7. Because $4 < 7$, **P2 preempts P1**. P1 returns to ready pool.
- $t=2$: P3 arrives with burst 2. P2 has remaining burst 3. Because $2 < 3$, **P3 preempts P2**. P2 returns to ready pool.
- $t=3$: P4 arrives with burst 1. P3 has remaining burst 1. Because $1 \not< 1$, **no preemption occurs**. P3 continues.
- $t=4$: P3 completes ($CT=4$). Ready candidates: P4 (rem 1), P2 (rem 3), P1 (rem 7). P4 runs.
- $t=5$: P4 completes ($CT=5$). P2 runs.
- $t=8$: P2 completes ($CT=8$). P1 runs.
- $t=15$: P1 completes ($CT=15$).

**Gantt Chart**:
```text
+---+---+---+---+-----+-------+
|P1 |P2 |P3 |P4 | P2  |  P1   |
+---+---+---+---+-----+-------+
0   1   2   4   5     8       15
```

**Result Metrics**:
- P1: $CT=15$, $TAT=15$, $WT=7$, $RT=0$ (first started at 0)
- P2: $CT=8$, $TAT=7$, $WT=3$, $RT=0$ (first started at 1)
- P3: $CT=4$, $TAT=2$, $WT=0$, $RT=0$ (first started at 2)
- P4: $CT=5$, $TAT=2$, $WT=1$, $RT=1$ (first started at 4)
- **Averages**: $TAT_{\text{avg}} = 6.50$, $WT_{\text{avg}} = 2.75$, $RT_{\text{avg}} = 0.25$

---

## 5. Round Robin (RR)

### 5.1 Principle & Philosophy
Round Robin is designed for time-sharing systems. Each process is assigned a fixed time slice called a **time quantum** ($q$). Ready processes are served in a circular FIFO queue.

- **Preemption**: **Preemptive**. When a process consumes its time quantum, it is preempted and rotated to the tail of the ready queue.
- **Quantum Expiration & Arrival Ordering**: When a process completes its quantum at timestamp $t$, any new processes arriving at timestamp $t$ are admitted into the ready queue **before** the preempted process is requeued. This ensures fairness to newly arriving jobs.
- **Timeline Normalization**: If a process finishes its quantum and no other process is waiting in the ready queue, it retains the CPU. Consecutive slices of the same process are merged into a single unbroken timeline segment.

### 5.2 Selection & Tie-Breaking Rules
1. **Primary**: Front of the FIFO ready queue.
2. **Simultaneous Arrival Tie-Breaker**: Relative input workload order.

### 5.3 Configuration Options
- **Time Quantum ($q$)**: Positive integer ($q > 0$).

### 5.4 Worked Example
**Workload**: Quantum $q = 2$.
| Process | Arrival Time | Burst Time |
| :---: | :---: | :---: |
| **P1** | 0 | 5 |
| **P2** | 1 | 3 |
| **P3** | 2 | 1 |

**Execution Trace**:
- $t=0$: Queue: `[P1]`. P1 runs.
- $t=1$: P2 arrives. Queue: `[P1(running), P2]`.
- $t=2$: P3 arrives. P1 quantum expires (2 units used, 3 remaining). P3 is admitted first, then P1 is requeued. Queue: `[P2, P3, P1]`.
- $t=2$: P2 runs for quantum 2 (until $t=4$).
- $t=4$: P2 quantum expires (1 unit remaining). Queue: `[P3, P1, P2]`.
- $t=4$: P3 runs. Burst is 1, so P3 finishes early at $t=5$ ($CT=5$). Queue: `[P1, P2]`.
- $t=5$: P1 runs for quantum 2 (until $t=7$, 1 remaining). Requeued. Queue: `[P2, P1]`.
- $t=7$: P2 runs for 1 unit and completes at $t=8$ ($CT=8$). Queue: `[P1]`.
- $t=8$: P1 runs for final 1 unit and completes at $t=9$ ($CT=9$).

**Gantt Chart**:
```text
+---+---+---+---+---+---+
|P1 |P2 |P3 |P1 |P2 |P1 |
+---+---+---+---+---+---+
0   2   4   5   7   8   9
```

**Result Metrics**:
- P1: $CT=9$, $TAT=9$, $WT=4$, $RT=0$
- P2: $CT=8$, $TAT=7$, $WT=4$, $RT=1$
- P3: $CT=5$, $TAT=3$, $WT=2$, $RT=2$
- **Averages**: $TAT_{\text{avg}} = 6.33$, $WT_{\text{avg}} = 3.33$, $RT_{\text{avg}} = 1.00$

---

## 6. Non-Preemptive Priority Scheduling

### 6.1 Principle & Philosophy
Allocates the CPU to the ready process with the highest priority. Once dispatched, the process runs to completion without interruption.

- **Priority Direction**: **Smaller numerical value = higher priority** (`1 > 2`, `0 > 1`, `-5 > 0`).
- **Preemption**: **Non-preemptive**.

### 6.2 Selection & Tie-Breaking Rules
1. **Primary**: Highest priority (smallest numerical value).
2. **Secondary**: Earlier arrival time ($AT$).
3. **Tertiary**: Input sequence order.

### 6.3 Configuration Options
- Process priority integer (specified per process).

### 6.4 Worked Example
**Workload**:
| Process | Arrival Time | Burst Time | Priority |
| :---: | :---: | :---: | :---: |
| **P1** | 0 | 4 | 2 |
| **P2** | 1 | 3 | 1 |
| **P3** | 2 | 5 | 0 |

**Execution Trace**:
- $t=0$: Only P1 has arrived. P1 runs.
- $t=1$: P2 arrives (Priority 1). P1 is non-preemptive and continues.
- $t=2$: P3 arrives (Priority 0). P1 continues.
- $t=4$: P1 completes ($CT=4$). Ready queue: P3 (Priority 0), P2 (Priority 1). P3 has higher priority (`0 < 1`). P3 runs.
- $t=9$: P3 completes ($CT=9$). P2 runs.
- $t=12$: P2 completes ($CT=12$).

**Gantt Chart**:
```text
+-----+-------+-----+
| P1  |  P3   | P2  |
+-----+-------+-----+
0     4       9     12
```

---

## 7. Preemptive Priority Scheduling

### 7.1 Principle & Philosophy
Event-driven preemptive scheduler allocating the CPU to the highest-priority ready process.

- **Preemption Rule**: When a new process arrives, it preempts the currently executing process **if and only if** its priority is **strictly higher** (`new.priority < running.priority`).
- **Equal Priority Behavior**: Equal-priority arrivals do **not** preempt the running process, preventing priority thrashing.

### 7.2 Selection & Tie-Breaking Rules
1. **Primary**: Highest priority (smallest numerical value).
2. **Secondary**: Earlier arrival time.
3. **Tertiary**: Input sequence order.

### 7.3 Worked Example
Using the same workload as Section 6.4:
| Process | Arrival Time | Burst Time | Priority |
| :---: | :---: | :---: | :---: |
| **P1** | 0 | 4 | 2 |
| **P2** | 1 | 3 | 1 |
| **P3** | 2 | 5 | 0 |

**Execution Trace**:
- $t=0$: P1 starts running (remaining: 4, Priority 2).
- $t=1$: P2 arrives (Priority 1). Since $1 < 2$, **P2 preempts P1**. P1 returns to ready pool with 3 units remaining.
- $t=2$: P3 arrives (Priority 0). Since $0 < 1$, **P3 preempts P2**. P2 returns to ready pool with 2 units remaining.
- $t=7$: P3 completes ($CT=7$). Ready queue: P2 (Priority 1, rem 2), P1 (Priority 2, rem 3). P2 runs.
- $t=9$: P2 completes ($CT=9$). P1 runs.
- $t=12$: P1 completes ($CT=12$).

**Gantt Chart**:
```text
+---+---+---------+---+-----+
|P1 |P2 |   P3    |P2 | P1  |
+---+---+---------+---+-----+
0   1   2         7   9     12
```

**Comparison with Non-Preemptive**:
- In the preemptive version, high-priority P3 completes at $t=7$ (compared to $t=9$ in non-preemptive), significantly improving response and turnaround time for critical jobs.

---

## 8. Multilevel Queue (MLQ)

### 8.1 Principle & Philosophy
Multilevel Queue partitions the ready pool into multiple distinct queues, typically corresponding to process categories (e.g., interactive foreground vs. batch background).

- **Permanent Process Binding**: Each process is permanently bound to exactly one queue throughout the simulation (`MlqProcessBinding`). Processes do not migrate between queues.
- **Decoupled Priorities**:
  - **Queue Priority**: Governs which queue receives the CPU (smaller numerical value = higher queue priority).
  - **Process Priority**: Governs ordering inside a priority-based queue.
- **Supported Queue Algorithms**: Each queue independently runs one of the following: FCFS, SJF, SRTF, Round Robin (with dedicated internal quantum $q_{\text{internal}}$), Non-Preemptive Priority, or Preemptive Priority.

### 8.2 Inter-Queue Scheduling Policies
1. **Fixed Priority Preemptive (`FixedPriorityPreemptive`)**:
   - The highest-priority non-empty queue always runs.
   - An arrival into a higher-priority queue immediately preempts execution in a lower-priority queue.
2. **Round Robin Between Queues (`RoundRobin`)**:
   - The scheduler cycles through non-empty queues with an inter-queue time quantum ($Q_{\text{inter}}$).
   - In nested Round Robin configurations, the internal slice and inter-queue slice decrease concurrently. State preservation allows an interrupted process to resume its remaining internal slice upon queue re-entry.

### 8.3 Worked Example
**Configuration**:
- Queue `Q0` (Priority 0): Algorithm = Round Robin ($q=2$).
- Queue `Q1` (Priority 1): Algorithm = FCFS.
- Policy: `FixedPriorityPreemptive`.

**Workload**:
- P1 bound to `Q0`: $AT=0, BT=4$
- P2 bound to `Q1`: $AT=0, BT=3$

**Execution Trace**:
- $t=0$: Q0 is non-empty. P1 runs for quantum 2 (until $t=2$).
- $t=2$: P1 still has 2 units remaining in Q0. Since Q0 still has ready work, P1 continues running in Q0 until $t=4$ ($CT=4$).
- $t=4$: Q0 is empty. Q1 is now selected. P2 runs to completion at $t=7$ ($CT=7$).

**Gantt Chart**:
```text
+-------+-----+
|  P1   | P2  |
+-------+-----+
0       4     7
```

---

## 9. Multilevel Feedback Queue (MLFQ)

### 9.1 Principle & Philosophy
Multilevel Feedback Queue addresses the primary limitation of MLQ by allowing processes to **dynamically move between queues** based on their observed CPU behavior.

- **Initial Admission**: Every newly arrived process unconditionally enters Queue 0 (highest priority).
- **Strict Priority Hierarchy**: Queue 0 is highest; Queue $L-1$ is lowest. A process at level $k$ is scheduled only when queues $0 \dots k-1$ are empty.
- **Dynamic Demotion via CPU Allotment**:
  - Each level $l$ has a time quantum and a CPU allotment (defaulting to the quantum).
  - When a process accumulates execution at level $l$ equal to that level's allotment, it is demoted to level $l+1$ and appended to the back of level $l+1$.
  - Bottom-level processes stay at level $L-1$ upon allotment expiration, rotating in round-robin fashion.
- **Preemption & Allotment Preservation**:
  - When an executing process at level $l > 0$ is preempted by a higher-priority arrival into Queue 0, its consumed allotment and remaining quantum at level $l$ are **not** reset.
  - The preempted process returns to the **head** of level $l$ (`push_front`) to resume immediately when higher levels clear.
- **Starvation Prevention via Periodic Priority Boost**:
  - To prevent CPU-bound jobs from starving at the bottom queue, an optional periodic boost interval ($T_{\text{boost}}$) resets all uncompleted processes back to Queue 0 at $t = k \times T_{\text{boost}}$.
  - All consumed allotments are reset, giving older processes renewed opportunity to run at top priority.

### 9.2 Worked Example
**Configuration**:
- 2 levels: Level 0 ($q=2$, allotment=2), Level 1 ($q=4$, allotment=4).
- No boost.

**Workload**:
- P1 (CPU-bound): $AT=0, BT=5$
- P2 (Short interactive): $AT=3, BT=1$

**Execution Trace**:
- $t=0$: P1 enters Level 0 and runs for quantum 2.
- $t=2$: P1 has consumed 2 units at Level 0 (allotment exhausted). P1 is demoted to Level 1 (remaining burst 3).
- $t=2$: Level 0 is empty. P1 runs at Level 1.
- $t=3$: P2 arrives and enters Level 0! Because Level 0 has higher priority, **P2 preempts P1**. P1 consumed 1 unit at Level 1 (3 allotment remaining, 2 burst remaining) and pauses at head of Level 1.
- $t=3$: P2 runs at Level 0 for 1 unit and completes at $t=4$ ($CT=4$).
- $t=4$: Level 0 is empty. P1 resumes at Level 1 and completes at $t=6$ ($CT=6$).

**Gantt Chart**:
```text
+-----+---+---+-----+
| P1  |P1 |P2 | P1  |
+-----+---+---+-----+
0     2   3   4     6
```
*(Timeline normalization merges consecutive slices when no intervening process executes).*

---

## 10. Algorithm Comparison Matrix

| Algorithm | Preemptive? | Starvation Risk? | Best Suited For | Primary Advantage | Primary Trade-off |
| :--- | :---: | :---: | :--- | :--- | :--- |
| **FCFS** | No | No | Batch systems with equal-length jobs | Minimal scheduling overhead | High average waiting time (Convoy Effect) |
| **SJF** | No | Yes (long jobs) | Batch systems with known burst lengths | Optimal average waiting time (non-preemptive) | Requires knowing burst in advance |
| **SRTF** | Yes | Yes (long jobs) | Interactive systems with known job sizes | Minimizes average waiting time | Frequent context switching |
| **Round Robin** | Yes | No | General time-sharing and desktop OS | Excellent response time and fairness | Sensitive to quantum size choice |
| **Priority (NP)** | No | Yes (low prio) | Static priority environments | Prioritizes mission-critical tasks | Low-priority jobs can starve |
| **Priority (P)** | Yes | Yes (low prio) | Real-time and interrupt-driven systems | Immediate response to urgent tasks | Requires preemption overhead |
| **MLQ** | Configurable | Yes (low queues) | Systems with distinct process classes | Tailored scheduling per process type | Rigid; no migration across classes |
| **MLFQ** | Yes | No (with boost) | Modern general-purpose operating systems | Adapts automatically to job behavior | Most complex configuration |
