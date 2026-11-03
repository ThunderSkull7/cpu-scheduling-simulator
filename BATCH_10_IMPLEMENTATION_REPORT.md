# CPU Scheduling Simulator — Batch 10: Interactive CLI Implementation Report

## 1. Summary of Implementation

Batch 10 delivers a complete, robust, and decoupled interactive Command-Line Interface (CLI) application layer for the C++20 CPU Scheduling Simulator.

The CLI acts as a dedicated presentation and interaction layer that routes all scheduling execution exclusively through the unified `cpu_scheduler::SchedulingEngine` (`SchedulingRequest` / `ScheduleOutcome`), strictly avoiding any reimplementation or bypassing of underlying scheduling algorithms.

Key features implemented:
- **Interactive Main Menu**: Guided menu presenting options 1–8 for all supported scheduling algorithms (FCFS, SJF, SRTF, RR, Priority Non-Preemptive, Priority Preemptive, MLQ, MLFQ) and option 9 to exit cleanly.
- **Workload Collection & Input Validation**: Interactive prompt with field-level retry loops, validating process counts (1–100), process ID uniqueness (case-insensitive) and non-emptiness, non-negative arrival times ($\ge 0$), strictly positive burst times ($> 0$), priorities, and queue bindings.
- **Algorithm-Specific Configuration**:
  - Round Robin: prompts for strictly positive time quantum ($> 0$).
  - Multilevel Queue (MLQ): multi-queue setup (queue IDs, priorities, algorithms, internal RR quantum), inter-queue scheduling policy (Fixed-Priority Preemptive vs. Round Robin), and per-process queue bindings.
  - Multilevel Feedback Queue (MLFQ): multi-tier setup (2–10 levels), level-specific time quantums, optional custom CPU allotments ($\ge$ quantum), and optional periodic priority boosting.
- **Results Presentation**: Aligned tabular execution details, comprehensive simulation summary, aggregate scheduling metrics (turnaround, waiting, response times, CPU utilization), and chronological execution timeline trace.
- **Decoupled Architecture & Testing**: Stream-based design (`std::istream& in`, `std::ostream& out`) enabling full headless automated testability without manual interaction.
- **Zero Compiler Warnings**: Builds with zero warnings under MSVC `/W4 /permissive-` in both Debug and Release configurations.
- **Automated Test Suite**: 24 new comprehensive GoogleTest tests added in `tests/test_cli.cpp`, bringing total repository test count to **511 tests** (100% passing in both Debug and Release).

---

## 2. Files Added and Modified

### Added Files:
- `include/cpu_scheduler/cli.hpp`: Declarations for `CliController`, formatting utilities, safe parsers, and interactive workflow prompt methods.
- `src/cli.cpp`: Implementation of `CliController`, menu navigation, workload collectors, algorithm-specific configuration handlers, result formatters, and retry loops.
- `src/cli_main.cpp`: CLI executable entry point (`main()`) connecting standard streams (`std::cin`, `std::cout`) to `CliController`.
- `tests/test_cli.cpp`: Comprehensive unit and integration tests (24 tests) validating menu selection, input validation, EOF handling, all 8 algorithm configurations and executions, and formatting utilities.
- `BATCH_10_IMPLEMENTATION_REPORT.md`: This comprehensive implementation report.

### Modified Files:
- `CMakeLists.txt`: Added `src/cli.cpp` to `cpu_scheduler_lib`; added `cpu_scheduler_cli` executable target linked to `cpu_scheduler_lib` with `/W4 /permissive-` compile flags.
- `tests/CMakeLists.txt`: Added `test_cli.cpp` to `cpu_scheduler_tests` target.
- `README.md`: Updated project status, test count (511 tests), CLI build and run instructions, interactive user guide, example workflow, and project directory structure.
- `docs/ARCHITECTURE.md`: Added architectural documentation for `CliController` (Section 2.15).

---

## 3. CLI Architecture and Application Flow

The CLI is structured into decoupled components:

```
+-----------------------------------------------------------+
|                       cli_main.cpp                        |
|        (std::cin, std::cout -> CliController::run)        |
+-----------------------------+-----------------------------+
                              |
                              v
+-----------------------------------------------------------+
|                  cpu_scheduler::CliController             |
|  - read_line, read_int, read_int64, read_non_empty_string |
|  - prompt_menu_choice, prompt_workload, prompt_*_config   |
|  - format_banner, format_summary, format_process_table    |
|  - format_aggregate_metrics, format_timeline, format_error|
+-----------------------------+-----------------------------+
                              |
                              | builds SchedulingRequest
                              v
+-----------------------------------------------------------+
|              cpu_scheduler::SchedulingEngine              |
|                     (execute / schedule)                  |
+-----------------------------+-----------------------------+
                              |
                              | dispatches to
                              v
+-----------------------------------------------------------+
|       Underlying Schedulers (FCFS, SJF, SRTF, RR,         |
|              Priority NP/P, MLQ, MLFQ)                    |
+-----------------------------------------------------------+
```

### Application Execution Flow:
1. **Banner & Main Menu**: Displays application banner and lists options 1–9.
2. **Algorithm Routing**:
   - For FCFS / SJF / SRTF: collects standard workload.
   - For Round Robin: prompts for positive quantum, then collects workload.
   - For Non-Preemptive / Preemptive Priority: collects workload with per-process priority values.
   - For MLQ: prompts for queue definitions and inter-queue policy, then collects workload with queue assignments.
   - For MLFQ: prompts for level parameters (quantums, optional allotments, optional boost), then collects workload.
3. **Dispatch**: Constructs `SchedulingRequest` and invokes `SchedulingEngine::execute(request)`.
4. **Presentation**: Formats Simulation Summary, Process Execution Details table, Aggregate Metrics, and chronological Execution Timeline.
5. **Post-Simulation Options**:
   - `1. Run another simulation`: Clears state and loops back to Main Menu.
   - `2. Exit`: Exits gracefully with code 0.
   - Stream EOF: Cleanly unwinds and terminates with code 0 without infinite loops.

---

## 4. Algorithm Configuration and Invocation

Each algorithm variant is configured and dispatched through `SchedulingEngine`:

| Algorithm | Menu Choice | Configuration Inputs | Engine Invocation |
| :--- | :---: | :--- | :--- |
| **FCFS** | `1` | Workload (ID, arrival, burst) | `SchedulingRequest(SchedulingAlgorithm::Fcfs, workload)` |
| **SJF** (Non-Preemptive) | `2` | Workload (ID, arrival, burst) | `SchedulingRequest(SchedulingAlgorithm::Sjf, workload)` |
| **SRTF** (Preemptive) | `3` | Workload (ID, arrival, burst) | `SchedulingRequest(SchedulingAlgorithm::Srtf, workload)` |
| **Round Robin** | `4` | Time quantum ($q > 0$), workload | `SchedulingRequest::round_robin(workload, quantum)` |
| **Priority** (Non-Preemptive) | `5` | Workload with process priorities | `SchedulingRequest(SchedulingAlgorithm::NonPreemptivePriority, workload)` |
| **Priority** (Preemptive) | `6` | Workload with process priorities | `SchedulingRequest(SchedulingAlgorithm::PreemptivePriority, workload)` |
| **Multilevel Queue (MLQ)** | `7` | Queue definitions (IDs, priorities, algorithms, quantum), inter-queue policy, process bindings | `SchedulingRequest::mlq(mlq_workload)` |
| **Multilevel Feedback Queue (MLFQ)** | `8` | Level quantums, optional allotments, optional boost interval, workload | `SchedulingRequest::mlfq(workload, mlfq_config)` |

---

## 5. Input Validation and Error-Handling Strategy

The CLI implements field-level validation and recovery loops to prevent stream corruption or uncaught exceptions:

1. **Safe Low-Level Stream Parsing**:
   - `read_line()` consumes full lines via `std::getline` and trims whitespace (including `\r` and `\n`).
   - If stream reaches EOF, immediately returns `std::nullopt` to cleanly exit without looping.
2. **Numeric Parsing & Range Protection**:
   - `read_int64()` and `read_int()` use `std::stoll` with index verification and bound checks, protecting against non-numeric characters, empty input, trailing garbage, and arithmetic overflow.
   - Re-prompts the exact field on failure with informative diagnostics (e.g. `Error: Invalid input. Value must be an integer between <min> and <max>. Please try again.`).
3. **Process Constraints**:
   - Process count bounded to $[1, 100]$.
   - Process IDs validated against empty/whitespace input and checked for workload uniqueness (case-insensitive via `ProcessValidator::normalize_id`).
   - Arrival times checked for non-negativity ($\ge 0$).
   - Burst times checked for strict positivity ($> 0$).
4. **Queue Integrity**:
   - In MLQ, queue IDs are verified unique and non-empty.
   - Process queue assignments are verified against defined queue IDs with case-insensitive matching.
5. **Engine Error Presentation**:
   - Unsuccessful scheduling outcomes from `SchedulingEngine` (such as domain validation errors or arithmetic overflow) are captured and formatted with `format_error()`, displaying both primary error messages and individual validation failure items.

---

## 6. Results Presentation

The CLI provides clear, human-readable terminal output:

### 1. Simulation Summary
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
```

### 2. Process Details Table
```text
Process Execution Details:
PID       Arrival   Burst   Priority     Queue  Completion  Turnaround   Waiting  Response
------------------------------------------------------------------------------
P1              0       3          0        Q1           3           3         0         0
P2              1       4          0        Q2           7           6         2         2
------------------------------------------------------------------------------
```
*(Priority column displayed when relevant to Priority or MLQ algorithms; Queue column displayed for MLQ).*

### 3. Aggregate Metrics
```text
============================================================
                     AGGREGATE METRICS
============================================================
Average Turnaround Time:    4.50
Average Waiting Time:       1.00
Average Response Time:      1.00
CPU Utilization:            100.00%
============================================================
```

### 4. Chronological Timeline
```text
Execution Timeline:
  [0 - 3: P1] [3 - 7: P2]
```
*(Explicitly shows idle slices as `[start - end: IDLE]` when CPU is idle).*

---

## 7. CMake Integration and Launch Instructions

### Build Configuration:
In root `CMakeLists.txt`:
```cmake
add_library(cpu_scheduler_lib
    ...
    src/cli.cpp
)

add_executable(cpu_scheduler_cli
    src/cli_main.cpp
)

target_link_libraries(cpu_scheduler_cli
    PRIVATE
        cpu_scheduler_lib
)
```

### Compilation:
```powershell
# Debug Build
cmake --build --preset debug

# Release Build
cmake --build --preset release
```

### Launch Instructions:
```powershell
# Run Debug CLI
.\build\debug\Debug\cpu_scheduler_cli.exe

# Run Release CLI
.\build\release\Release\cpu_scheduler_cli.exe
```

---

## 8. Test Coverage and Exact Results

Automated unit tests added in `tests/test_cli.cpp` (24 new tests):
- `CliTest.RunWithOption9ExitsGracefully`: Menu option 9 exits with return code 0.
- `CliTest.RunWithImmediateEofExitsGracefully`: Immediate EOF terminates gracefully with code 0.
- `CliTest.MenuInvalidSelectionRetriesAndExits`: Non-numeric, negative, and out-of-range menu inputs re-prompt and exit.
- `CliTest.FcfsEndToEndExecution`: Complete FCFS simulation workflow via CLI.
- `CliTest.SjfEndToEndExecution`: Complete Non-Preemptive SJF simulation workflow via CLI.
- `CliTest.SrtfEndToEndExecution`: Complete Preemptive SRTF simulation workflow via CLI.
- `CliTest.RoundRobinEndToEndExecution`: Complete Round Robin simulation with quantum prompt.
- `CliTest.NonPreemptivePriorityEndToEndExecution`: Complete Priority NP simulation with priority prompts.
- `CliTest.PreemptivePriorityEndToEndExecution`: Complete Priority Preemptive simulation with priority prompts.
- `CliTest.MlqEndToEndExecution`: Complete MLQ simulation with queue configuration, inter-queue policy, and process bindings.
- `CliTest.MlfqEndToEndExecution`: Complete MLFQ simulation with level setup, quantums, and execution.
- `CliTest.MultipleSimulationsInOneSession`: Running multiple consecutive simulations (FCFS then SJF) in one interactive session without state leakage.
- `CliTest.WorkloadValidationInvalidProcessCount`: Rejection and retry of invalid process counts (-1, 0, 101, text).
- `CliTest.WorkloadValidationDuplicateProcessId`: Rejection of duplicate process IDs (case-insensitive).
- `CliTest.WorkloadValidationNegativeArrivalAndBurst`: Rejection of negative arrival times and zero/negative burst times.
- `CliTest.RoundRobinInvalidQuantumRejection`: Rejection of zero, negative, and non-numeric RR quantums.
- `CliTest.MlqInvalidQueueBindingAndDuplicateQueueId`: Rejection of duplicate queue IDs and non-existent queue assignments.
- `CliTest.MlfqCustomAllotmentAndBoostEnabled`: Complete MLFQ workflow with custom CPU allotments and priority boost enabled.
- `CliTest.FormatErrorDisplaysMessageAndValidationDetails`: Verifies error formatting and validation diagnostics.
- `CliTest.FormatTimelineIdlePeriod`: Verifies idle intervals formatted as `[start - end: IDLE]`.
- `CliTest.FormatTimelineEmpty`: Verifies empty timeline formatting.
- `CliTest.EofDuringWorkloadInputExitsCleanly`: Stream EOF during workload input terminates gracefully.
- `CliTest.EofDuringMlfqConfigExitsCleanly`: Stream EOF during MLFQ config terminates gracefully.
- `CliTest.EofDuringMlqConfigExitsCleanly`: Stream EOF during MLQ config terminates gracefully.

---

## 9. Debug and Release Verification

### Test Counts:
- Baseline tests (Batches 1–9): **487 tests**
- New tests (Batch 10): **24 tests**
- Total test count: **511 tests**
- Skipped tests: **0**

### Verification Summary:
- **Debug Build (`debug` preset)**:
  - Compiler: MSVC (`/W4 /permissive-`)
  - Warnings: **0 warnings**
  - CTest (`debug-test` preset): **511 / 511 passed (100%)**, total test time: 15.81s.
- **Release Build (`release` preset)**:
  - Compiler: MSVC (`/W4 /permissive-`)
  - Warnings: **0 warnings**
  - CTest (`release-test` preset): **511 / 511 passed (100%)**, total test time: 14.66s.

---

## 10. End-to-End Scripted Runs

All eight scheduling algorithm variants were verified by executing the compiled executable `.\build\debug\Debug\cpu_scheduler_cli.exe` with scripted stdin feeds:

### 1. FCFS
- Input: Choice 1 (FCFS), 2 processes (P1: arrival 0, burst 3; P2: arrival 1, burst 4), post-sim choice 2 (Exit).
- Result: Total time 7, CPU utilization 100%, timeline `[0 - 3: P1] [3 - 7: P2]`, average turnaround 4.50, average waiting 1.00. Exit code 0.

### 2. SJF (Non-Preemptive)
- Input: Choice 2 (SJF), 2 processes (P1: arrival 0, burst 5; P2: arrival 1, burst 2), post-sim choice 2 (Exit).
- Result: Total time 7, CPU utilization 100%, timeline `[0 - 5: P1] [5 - 7: P2]`, average turnaround 5.50, average waiting 2.00. Exit code 0.

### 3. SRTF (Preemptive)
- Input: Choice 3 (SRTF), 2 processes (P1: arrival 0, burst 6; P2: arrival 2, burst 2), post-sim choice 2 (Exit).
- Result: Total time 8, CPU utilization 100%, timeline `[0 - 2: P1] [2 - 4: P2] [4 - 8: P1]`, average turnaround 5.00, average waiting 1.00. Exit code 0.

### 4. Round Robin (RR)
- Input: Choice 4 (RR), quantum 2, 2 processes (P1: arrival 0, burst 3; P2: arrival 1, burst 3), post-sim choice 2 (Exit).
- Result: Total time 6, CPU utilization 100%, timeline `[0 - 2: P1] [2 - 4: P2] [4 - 5: P1] [5 - 6: P2]`, average turnaround 5.00, average waiting 2.00. Exit code 0.

### 5. Priority (Non-Preemptive)
- Input: Choice 5 (Priority NP), 2 processes (P1: arrival 0, burst 4, priority 2; P2: arrival 1, burst 3, priority 1), post-sim choice 2 (Exit).
- Result: Total time 7, CPU utilization 100%, timeline `[0 - 4: P1] [4 - 7: P2]`, average turnaround 5.00, average waiting 1.50. Exit code 0.

### 6. Priority (Preemptive)
- Input: Choice 6 (Priority Preemptive), 2 processes (P1: arrival 0, burst 6, priority 2; P2: arrival 2, burst 2, priority 1), post-sim choice 2 (Exit).
- Result: Total time 8, CPU utilization 100%, timeline `[0 - 2: P1] [2 - 4: P2] [4 - 8: P1]`, average turnaround 5.00, average waiting 1.00. Exit code 0.

### 7. Multilevel Queue (MLQ)
- Input: Choice 7 (MLQ), 2 queues (Q1: priority 0, FCFS; Q2: priority 1, FCFS), policy 1 (Fixed Priority Preemptive), 2 processes (P1: arrival 0, burst 3, priority 0, queue Q1; P2: arrival 1, burst 4, priority 0, queue Q2), post-sim choice 2 (Exit).
- Result: Total time 7, CPU utilization 100%, timeline `[0 - 3: P1] [3 - 7: P2]`, process table displays both Priority and Queue columns. Exit code 0.

### 8. Multilevel Feedback Queue (MLFQ)
- Input: Choice 8 (MLFQ), 2 levels (Level 0: quantum 2, no custom allotment; Level 1: quantum 4, no custom allotment), no boost, 1 process (P1: arrival 0, burst 5), post-sim choice 2 (Exit).
- Result: Total time 5, CPU utilization 100%, timeline `[0 - 2: P1] [2 - 5: P1]`, showing demotion from Level 0 to Level 1. Exit code 0.

---

## 11. Compiler Warnings

Both Debug and Release configurations compiled with **zero warnings** under MSVC flags `/W4 /permissive-`:
- `cpu_scheduler_lib`: 0 warnings
- `cpu_scheduler_cli`: 0 warnings
- `cpu_scheduler_tests`: 0 warnings
- `basic_usage`: 0 warnings

---

## 12. Known Limitations and Deferred Work

In strict accordance with the Batch 10 prompt scope boundaries:
1. **Dedicated Graphical / Advanced Gantt Chart Visualization**: Reserved for Batch 11. Current output provides structured textual tables and chronological execution segment ranges.
2. **File Ingestion / Export (CSV, JSON)**: Reserved for Batches 11–12.
3. **Comprehensive User Manual**: Full user manual documentation is reserved for Batch 14.

---

## 13. Git Baseline and Commit Integrity

- Baseline parent commit (Batch 9): `c06feb67c17554ef67966eac1fe89a7aaf9d1a3f`
- Branch: `main`
- Historical preserved audit reports:
  - `BATCH_5_INDEPENDENT_AUDIT_REPORT.md` (untracked, intact)
  - `BATCH_6_INDEPENDENT_AUDIT_REPORT.md` (untracked, intact)
  - `BATCH_7_INDEPENDENT_AUDIT_REPORT.md` (untracked, intact)
  - `BATCH_8_INDEPENDENT_AUDIT_REPORT.md` (untracked, intact)
  - `BATCH_9_INDEPENDENT_AUDIT_REPORT.md` (untracked, intact)
- Commit message: `feat: implement interactive CPU scheduling CLI`
- Implementation commit hash: [To be generated upon git commit]
