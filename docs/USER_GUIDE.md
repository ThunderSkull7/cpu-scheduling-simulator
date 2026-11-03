# CPU Scheduling Simulator — User Guide

A practical, beginner-friendly manual for building, running, and using the C++20 CPU Scheduling Simulator on Windows.

---

## Table of Contents

1. [Prerequisites and System Requirements](#1-prerequisites-and-system-requirements)
2. [Building the Project on Windows](#2-building-the-project-on-windows)
3. [Running the Application](#3-running-the-application)
4. [Navigating the Main Menu](#4-navigating-the-main-menu)
5. [Entering Process Workloads Manually](#5-entering-process-workloads-manually)
6. [Configuring Scheduling Algorithms](#6-configuring-scheduling-algorithms)
   - [First Come, First Served (FCFS)](#first-come-first-served-fcfs)
   - [Shortest Job First (SJF, Non-Preemptive)](#shortest-job-first-sjf-non-preemptive)
   - [Shortest Remaining Time First (SRTF, Preemptive)](#shortest-remaining-time-first-srtf-preemptive)
   - [Round Robin (RR)](#round-robin-rr)
   - [Priority Schedulers (Non-Preemptive & Preemptive)](#priority-schedulers-non-preemptive--preemptive)
   - [Multilevel Queue (MLQ)](#multilevel-queue-mlq)
   - [Multilevel Feedback Queue (MLFQ)](#multilevel-feedback-queue-mlfq)
7. [Importing Workloads from CSV](#7-importing-workloads-from-csv)
8. [Exporting Scheduling Results to CSV](#8-exporting-scheduling-results-to-csv)
9. [Understanding Simulation Results and Metrics](#9-understanding-simulation-results-and-metrics)
10. [Interpreting the Gantt Chart](#10-interpreting-the-gantt-chart)
11. [Common Input Errors and Recovery](#11-common-input-errors-and-recovery)
12. [Troubleshooting and Frequently Asked Questions](#12-troubleshooting-and-frequently-asked-questions)

---

## 1. Prerequisites and System Requirements

To build and run the simulator on Windows, install the following tools:

- **Operating System**: Windows 10 or Windows 11 (64-bit).
- **C++ Compiler**: Microsoft Visual C++ (MSVC) from **Visual Studio 2022** (version 17.0 or later) with the **Desktop development with C++** workload installed.
- **C++ Standard**: C++20 support (`/std:c++20 /permissive- /W4`).
- **Build System**: **CMake 3.20** or later (included with Visual Studio or installed via winget/installer).
- **Package Manager**: **vcpkg** (used in manifest mode for GoogleTest). Ensure the environment variable `VCPKG_ROOT` points to your vcpkg installation directory.
- **Console Terminal**: Windows Terminal, PowerShell 7+, or Command Prompt. For proper display of Unicode box-drawing characters, run `chcp 65001` to enable UTF-8 output.

---

## 2. Building the Project on Windows

The repository uses standard **CMake Presets** defined in `CMakePresets.json`.

### Step 1: Clone the Repository & Open Developer PowerShell
Open **Developer PowerShell for VS 2022** (or standard PowerShell with MSVC environment initialized).

```powershell
cd "D:\Project\CPU Schedulling"
```

Verify that `VCPKG_ROOT` is set in your environment:
```powershell
$env:VCPKG_ROOT
# Expected: e.g. C:\vcpkg or C:\tools\vcpkg
```

### Step 2: Configure with CMake Presets
Configure the build tree for either Debug or Release:

```powershell
# For Debug configuration (with debugging symbols):
cmake --preset windows-msvc-debug

# For Release configuration (optimized):
cmake --preset windows-msvc-release
```

### Step 3: Build the Targets
Build all libraries, executables, examples, and tests:

```powershell
# Build Debug:
cmake --build --preset debug

# Build Release:
cmake --build --preset release
```

Upon completion, CMake generates the following executables:
- **CLI Application**: `build\<preset>\Debug\cpu_scheduler_cli.exe` (or `Release\cpu_scheduler_cli.exe`)
- **Automated Test Suite**: `build\<preset>\tests\Debug\cpu_scheduler_tests.exe`
- **API Example Program**: `build\<preset>\examples\Debug\basic_usage.exe`

### Step 4: Run the Automated Tests
Verify that all 601 tests pass:

```powershell
# Run Debug tests:
ctest --preset debug-test

# Run Release tests:
ctest --preset release-test
```

Expected output: `100% tests passed out of 601`.

---

## 3. Running the Application

Launch the interactive CLI executable directly from your terminal:

```powershell
# For Debug build:
.\build\debug\Debug\cpu_scheduler_cli.exe

# For Release build:
.\build\release\Release\cpu_scheduler_cli.exe
```

Upon startup, the application presents the welcome banner and the algorithm selection menu:

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

---

## 4. Navigating the Main Menu

Enter an integer from `1` to `9` to make a selection:

- **Options 1 to 8**: Select a CPU scheduling algorithm.
- **Option 9**: Cleanly exits the simulator.

If you enter an invalid number, letters, or out-of-range values, the CLI displays an error message and reprompts:
```text
Enter selection (1-9): 12
Error: Invalid input. Please enter a valid integer between 1 and 9.
Enter selection (1-9):
```

At any input prompt, sending an **End-of-File (EOF)** signal (via `Ctrl+Z` followed by `Enter` on Windows, or `Ctrl+D` on Unix) cleanly terminates the session without hanging or crashing.

---

## 5. Entering Process Workloads Manually

Once you select an algorithm (e.g. `1` for FCFS), the CLI asks for the workload:

```text
--- Workload Configuration ---
Enter number of processes (1-100) or 0 to import from CSV: 3
```

- Enter a number from `1` to `100` to enter processes manually.
- Enter `0` to import processes from a CSV file (see [Section 7](#7-importing-workloads-from-csv)).

### Process Field Prompts
For each process, the CLI prompts for:

1. **Process ID**: A unique, non-empty text label (e.g., `P1`, `Browser`, `Compiler`).
   - Case-insensitive uniqueness is enforced (`p1` and `P1` are treated as duplicate).
   - Whitespace is automatically trimmed.
2. **Arrival Time**: The point in simulation time when the process enters the system ($t \ge 0$). Must be a non-negative 64-bit integer.
3. **Burst Time**: Total CPU execution time required ($BT > 0$). Must be a strictly positive integer.
4. **Priority** *(Promoted only for Priority and MLQ schedulers)*:
   - An integer value.
   - **Important Rule**: **Smaller numerical values represent higher scheduling priority** (`0` is higher priority than `1`; negative priorities like `-1` are higher than `0`).

#### Example Manual Entry Transcript (3 Processes for FCFS):
```text
Process 1 of 3:
  Process ID (e.g. P1): P1
  Arrival Time (>= 0): 0
  Burst Time (> 0): 5

Process 2 of 3:
  Process ID (e.g. P1): P2
  Arrival Time (>= 0): 2
  Burst Time (> 0): 3

Process 3 of 3:
  Process ID (e.g. P1): P3
  Arrival Time (>= 0): 4
  Burst Time (> 0): 1
```

---

## 6. Configuring Scheduling Algorithms

### First Come, First Served (FCFS)
- **Menu Option**: `1`
- **Parameters**: None.
- **Behavior**: Processes execute to completion in order of arrival time. Simultaneous arrivals are broken by input order.

### Shortest Job First (SJF, Non-Preemptive)
- **Menu Option**: `2`
- **Parameters**: None.
- **Behavior**: When the CPU becomes idle, the scheduler dispatches the process with the shortest burst time among all arrived processes. Once dispatched, a process runs uninterrupted to completion.

### Shortest Remaining Time First (SRTF, Preemptive)
- **Menu Option**: `3`
- **Parameters**: None.
- **Behavior**: The scheduler always executes the process with the smallest remaining burst time. A newly arrived process preempts the running process **if and only if** its remaining burst is strictly smaller (`new.remaining < running.remaining`).

### Round Robin (RR)
- **Menu Option**: `4`
- **Parameters**:
  ```text
  --- Round Robin Configuration ---
  Enter time quantum (> 0): 2
  ```
- **Behavior**: Processes run in a FIFO queue for at most the specified time quantum. Preempted processes return to the back of the ready queue. Processes arriving at the exact quantum expiration timestamp are admitted to the ready queue before the preempted process is requeued.

### Priority Schedulers (Non-Preemptive & Preemptive)
- **Menu Options**:
  - `5`: Priority (Non-Preemptive)
  - `6`: Priority (Preemptive)
- **Parameters**: Prompts for process `Priority` during workload entry.
- **Behavior**:
  - **Non-Preemptive**: Selects the ready process with the highest priority (lowest number) and runs it to completion.
  - **Preemptive**: Preempts the active process if an arriving process has a **strictly higher priority** (`new.priority < running.priority`).

### Multilevel Queue (MLQ)
- **Menu Option**: `7`
- **Workflow**:
  1. Specify the number of queues (1 to 10).
  2. For each queue, enter:
     - Queue ID (e.g. `Q_Foreground`, `Q_Background`)
     - Queue Priority (integer, smaller = higher priority)
     - Internal Algorithm: `1: FCFS`, `2: SJF`, `3: SRTF`, `4: Round Robin` (prompts for internal quantum), `5: Priority NP`, `6: Priority Preemptive`
  3. Select inter-queue policy:
     - `1: Fixed Priority Preemptive`: Higher-priority queues always preempt lower-priority queues.
     - `2: Round Robin`: Time slices between queues (prompts for inter-queue quantum).
  4. Workload entry: Enter processes and assign each to one of the configured Queue IDs.

### Multilevel Feedback Queue (MLFQ)
- **Menu Option**: `8`
- **Workflow**:
  1. Specify the number of priority levels (2 to 10). Level 0 is highest priority.
  2. For each level, enter:
     - Time quantum ($q > 0$).
     - Optional custom CPU allotment (default is the quantum; custom allotment must be $\ge q$).
  3. Starvation Boost:
     - Enable periodic priority boost? (`y`/`n`).
     - If enabled, enter boost interval in time units ($T > 0$).
  4. Workload entry: Enter processes (processes automatically begin in Queue 0).

---

## 7. Importing Workloads from CSV

Instead of typing processes manually, you can load a workload from an RFC 4180 compliant CSV file.

### How to Import
1. At the process count prompt, enter `0`:
   ```text
   Enter number of processes (1-100) or 0 to import from CSV: 0
   ```
2. Enter the path to your CSV file:
   ```text
   --- CSV Workload Import ---
   Enter CSV workload file path (or 'cancel' to return): examples/sample_workload.csv
   ```
3. If successful, the CLI confirms and proceeds directly to simulation:
   ```text
   Successfully imported 4 process(es) from 'examples/sample_workload.csv'.
   Executing simulation...
   ```

### CSV Schema Requirements
The CSV must include a header line. Headers are case-insensitive and whitespace-tolerant.

- **Required Columns**:
  - `process_id` (aliases: `pid`, `process`, `id`)
  - `arrival_time` (aliases: `arrival`, `at`)
  - `burst_time` (aliases: `burst`, `bt`)
- **Optional Columns**:
  - `priority` (aliases: `prio`; defaults to 0 if omitted)
  - `queue_id` (aliases: `queue`, `qid`; used for MLQ)

#### Verified Sample CSV: `sample_workload.csv`
```csv
process_id,arrival_time,burst_time
P1,0,5
P2,1,3
P3,2,8
P4,3,6
```

#### Verified Sample with Priorities: `sample_workload_with_priority.csv`
```csv
process_id,arrival_time,burst_time,priority
P1,0,4,2
P2,1,5,1
P3,2,2,3
P4,3,1,0
```

> [!NOTE]
> Ensure your CSV file is encoded in **UTF-8 without BOM**. If saved with a UTF-8 BOM, the leading BOM bytes will be attached to the first header name, causing a column mismatch error.

---

## 8. Exporting Scheduling Results to CSV

After any simulation completes, the CLI presents post-simulation options:

```text
------------------------------------------------------------
Options:
  1. Run another simulation
  2. Exit
  3. Export results to CSV
------------------------------------------------------------
Enter selection (1-3): 3
```

Select `3` to open the CSV Export menu:

```text
------------------------------------------------------------
CSV Export Options:
  1. Export process metrics CSV
  2. Export execution timeline CSV
  3. Export both to CSV
  4. Return
------------------------------------------------------------
Enter selection (1-4): 3
```

### Destination Prompts & Safe File Replacement
- Default paths are provided in brackets (`[default: process_metrics.csv]` and `[default: execution_timeline.csv]`). Press `Enter` to accept defaults, or enter a custom path.
- **Overwrite Protection**: If the destination file already exists, the simulator prompts for confirmation:
  ```text
  File 'process_metrics.csv' already exists. Overwrite? (y/n) [default: y]:
  ```
- **Windows-Safe Replacement**: On Windows, the exporter stages writes to a temporary file (`.tmp`) and invokes `ReplaceFileW`. If writing fails, your original file remains untouched.

### Formula Injection Protection
If process IDs or queue IDs contain spreadsheet formula trigger characters (`=`, `+`, `-`, `@`), the exporter prepends a safe single-quote apostrophe (`'`) in the exported CSV. In-memory data structures and numeric fields remain completely unmutated.

---

## 9. Understanding Simulation Results and Metrics

Upon execution, the simulator outputs four synchronized sections:

### 1. Simulation Summary
Displays high-level run metadata:
```text
============================================================
                    SIMULATION SUMMARY
============================================================
Algorithm:                First Come, First Served (FCFS)
Process Count:            3
Total Simulation Time:    9
CPU Busy Time:            9
CPU Idle Time:            0
CPU Utilization:          100.00%
============================================================
```

### 2. Process Execution Details Table
Provides individual process lifecycle metrics:
```text
Process Execution Details:
PID      Arrival   Burst  Completion  Turnaround   Waiting  Response
-----------------------------------------------------------------
P1             0       5           5           5         0         0
P2             2       3           8           6         3         3
P3             4       1           9           5         4         4
```

### Metric Definitions:
- **Arrival Time ($AT$)**: When the process entered the ready pool.
- **Burst Time ($BT$)**: CPU time required by the process.
- **Completion Time ($CT$)**: Timestamp when the process finished its final execution slice.
- **Turnaround Time ($TAT$)**: Total elapsed time from arrival to completion:
  $$\text{Turnaround Time} = CT - AT$$
- **Waiting Time ($WT$)**: Total time the process spent waiting in the ready queue without CPU execution:
  $$\text{Waiting Time} = TAT - BT$$
- **Response Time ($RT$)**: Duration from arrival until the process first received the CPU:
  $$\text{Response Time} = \text{First Execution Start} - AT$$

### 3. Aggregate Metrics
Reports system-wide averages and resource utilization:
```text
============================================================
                    AGGREGATE METRICS
============================================================
Average Turnaround Time:    5.33
Average Waiting Time:       2.33
Average Response Time:      2.33
CPU Busy Time:              9
CPU Idle Time:              0
CPU Utilization:            100.00%
============================================================
```

---

## 10. Interpreting the Gantt Chart

The simulator includes a duration-aware text Gantt chart generator:

```text
Gantt Chart:
+-------+-----+---+
|  P1   | P2  |P3 |
+-------+-----+---+
0       5     8   9
```

### Key Visual Features:
1. **Proportional Block Scaling**: Wider blocks represent longer durations. In the example above, `P1` (duration 5) is noticeably wider than `P3` (duration 1).
2. **Boundary-Aligned Timestamps**: Numbers on the timeline axis align directly under the block boundary markers (`+` or `|`), eliminating misreadings.
3. **Idle Interval Blocks**: If the CPU has no ready processes to run, the chart displays an explicit `Idle` block (e.g. `| Idle |`).
4. **Preemption Tracking**: In preemptive algorithms (SRTF, RR, Preemptive Priority, MLQ, MLFQ), a process may appear in multiple non-consecutive blocks.
5. **Multi-Row Wrapping**: If a schedule exceeds the configured row width (default 80 columns), it automatically wraps to subsequent rows, maintaining readability.
6. **Queue Labels**: For Multilevel Queue (MLQ) simulations, blocks display the queue ID alongside the process ID, e.g.:
   ```text
   +----------------+----------------+
   |  P1 (Q_High)   |  P2 (Q_High)   |
   +----------------+----------------+
   ```

> [!NOTE]
> **MLFQ Queue Label Behavior**: In Multilevel Feedback Queue (MLFQ), the data model records each process's *final* queue level upon completion. Per-slice historical queue transitions are not recorded in timeline segments. Consequently, MLFQ Gantt charts display process IDs without misleading historical slice labels.

---

## 11. Common Input Errors and Recovery

The CLI validates all input field-by-field and provides actionable recovery messages:

| Error Situation | CLI Behavior & Recovery |
| :--- | :--- |
| **Empty or Whitespace-only ID** | Displays `Error: Process ID cannot be empty or blank.` Reprompts for ID. |
| **Duplicate Process ID** | Compares IDs case-insensitively. Displays `Error: Process ID 'P1' already exists. Must be unique.` Reprompts. |
| **Negative Arrival Time** | Displays `Error: Arrival time must be non-negative (>= 0).` Reprompts. |
| **Zero or Negative Burst** | Displays `Error: Burst time must be strictly positive (> 0).` Reprompts. |
| **Non-Numeric Input** | Displays `Error: Please enter a valid integer.` Reprompts without terminating. |
| **Non-Existent CSV File** | Displays `Unable to open file '<path>'.` Allows entering a corrected path or typing `cancel`. |
| **Malformed CSV Quoting** | Displays 1-based syntax error diagnostics (e.g. `Line 3, Col 8: Unclosed quote`). Allows file correction. |

---

## 12. Troubleshooting and Frequently Asked Questions

### Q: Why do Gantt chart Unicode border characters display as question marks or strange symbols?
**A**: Windows Command Prompt and older PowerShell consoles default to Legacy OEM code pages (such as CP437). To display Unicode borders cleanly, switch your console to UTF-8 before running:
```powershell
chcp 65001
```
Alternatively, the simulator automatically uses ASCII mode (`+`, `-`, `|`) in standard CLI output, which renders portably across all Windows consoles.

### Q: Why did CSV import reject my file with "Missing required column 'process_id'" even though the header exists?
**A**: Your text editor likely saved the file with a **UTF-8 Byte Order Mark (BOM)** (`0xEF, 0xBB, 0xBF`). In Notepad or VS Code, choose **Save with Encoding -> UTF-8** (without BOM).

### Q: What is the maximum number of processes supported?
**A**: The simulator supports up to **100 processes** per simulation run. This boundary is enforced to guarantee fast interactive performance and readable terminal reports.

### Q: Where are exported CSV files saved?
**A**: If you specify a relative filename (e.g. `results.csv`), it is saved in your current working directory from which you launched `cpu_scheduler_cli.exe`.
