# CPU Scheduling Simulator

A modern, high-performance C++20 simulator for Operating System CPU scheduling algorithms. The project features eight scheduling engines, a unified execution dispatcher, duration-aware Gantt chart visualization, comprehensive metrics analysis, an interactive CLI, an RFC 4180 compliant CSV import/export pipeline, and an automated regression test suite with 601 tests.

---

## Key Features

- **Eight Standard Scheduling Algorithms**:
  - First Come, First Served (FCFS)
  - Shortest Job First (SJF, Non-Preemptive)
  - Shortest Remaining Time First (SRTF, Preemptive)
  - Round Robin (RR, with configurable time quantum)
  - Non-Preemptive Priority (smaller numerical value = higher priority)
  - Preemptive Priority (strict preemption on higher priority)
  - Multilevel Queue (MLQ, independent queue algorithms & configurable inter-queue policies)
  - Multilevel Feedback Queue (MLFQ, dynamic demotion, allotments, and periodic starvation boost)
- **Unified Scheduling Engine (`SchedulingEngine`)**: Central type-safe dispatcher providing a single entry point for all scheduling algorithms with zero duplication.
- **Duration-Aware Text Gantt Chart (`GanttChart`)**: Proportional block scaling, boundary-aligned timestamps, multi-row line wrapping, explicit idle interval tracking, ASCII and Unicode box-drawing support, and MLQ queue metadata visualization.
- **Dedicated Results Presentation Engine (`ResultsRenderer`)**: Standalone formatting engine composing structured Simulation Summaries, aligned process tables, aggregate metrics, timelines, and complete reports.
- **RFC 4180 CSV Workload Import & Export Subsystem**:
  - Ingests workload CSV files with case-insensitive column aliases (`process_id`, `arrival_time`, `burst_time`, `priority`, `queue_id`), RFC 4180 quoting, and 1-based row error diagnostics.
  - Exports process execution metrics and chronological timelines to spreadsheet-compatible CSV documents.
  - Formula injection mitigation (neutralizes `=`, `+`, `-`, `@` triggers with single quotes at export boundaries).
  - Windows-safe atomic staged file replacement via `ReplaceFileW`.
- **Interactive Command-Line Interface (`CliController`)**: Full-featured interactive terminal application with field-by-field input validation, error recovery, CSV import/export integration, and stream EOF safety.
- **Automated Regression Suite**: 601 GoogleTest unit and integration tests (100% pass rate in Debug and Release configurations with zero compiler warnings under MSVC `/W4 /permissive-`).

---

## Supported Scheduling Algorithms Summary

| Algorithm | Type | Configuration Parameters | Primary Advantage |
| :--- | :---: | :--- | :--- |
| **FCFS** | Non-Preemptive | None | Simple, starvation-free, zero overhead |
| **SJF** | Non-Preemptive | None | Minimizes average waiting time for non-preemptive workloads |
| **SRTF** | Preemptive | None | Minimizes average waiting time for interactive workloads |
| **Round Robin** | Preemptive | Time Quantum ($q > 0$) | Excellent response time and fairness |
| **Priority (NP)** | Non-Preemptive | Process Priority integer | Prioritizes mission-critical processes |
| **Priority (P)** | Preemptive | Process Priority integer | Immediate response to high-priority processes |
| **MLQ** | Configurable | Queues (1–10), algorithms, inter-queue policy | Separates distinct process categories |
| **MLFQ** | Preemptive | Levels (2–10), quantums, allotments, boost | Dynamic adaptation to CPU-bound vs interactive jobs |

For in-depth algorithm descriptions, tie-breaking rules, and worked examples, see the [Algorithm Reference](docs/ALGORITHM_REFERENCE.md).

---

## Technology Stack

- **Language Standard**: ISO C++20 (`/std:c++20 /permissive-`)
- **Build System**: CMake 3.20+ with `CMakePresets.json`
- **Compiler**: Microsoft Visual Studio 2022 (MSVC v143 x64) / Clang / GCC compatible
- **Package Management**: vcpkg (manifest mode via `vcpkg.json`)
- **Testing Framework**: GoogleTest (via vcpkg)
- **Standard Library Features**: `std::variant`, `std::span`, `std::optional`, `std::string_view`, concepts, `<filesystem>`, `<chrono>`, `<numeric>`.

---

## Repository Structure

```text
CPU Scheduling/
├── CMakeLists.txt                  # Root CMake configuration
├── CMakePresets.json               # Presets for configure, build, and test
├── vcpkg.json                      # Dependency manifest (GoogleTest)
├── README.md                       # Main project overview and quickstart
│
├── docs/                           # Comprehensive documentation
│   ├── USER_GUIDE.md               # Beginner-friendly guide for building and using CLI
│   ├── ALGORITHM_REFERENCE.md      # Theory, worked examples, and tie-breaking rules
│   ├── CSV_REFERENCE.md            # Workload/results CSV schema, security, and formats
│   └── ARCHITECTURE.md             # System design, component boundaries, and testing
│
├── include/cpu_scheduler/          # Public C++ headers
│   ├── process.hpp                 # Immutable workload process model
│   ├── process_validator.hpp       # Input validation engine
│   ├── process_state.hpp           # Dynamic process tracking & metrics
│   ├── execution_segment.hpp       # Timeline segment with idle intervals
│   ├── scheduling_result.hpp       # Results container and aggregate metrics
│   ├── fcfs_scheduler.hpp          # FCFS scheduling engine
│   ├── sjf_scheduler.hpp           # Non-preemptive SJF engine
│   ├── srtf_scheduler.hpp          # Preemptive SRTF engine
│   ├── rr_scheduler.hpp            # Round Robin engine
│   ├── priority_scheduler.hpp      # Priority scheduling engines (NP & Preemptive)
│   ├── mlq_scheduler.hpp           # Multilevel Queue engine
│   ├── mlfq_scheduler.hpp          # Multilevel Feedback Queue engine
│   ├── scheduling_engine.hpp       # Unified scheduling dispatcher & reflection
│   ├── gantt_chart.hpp             # Duration-aware Gantt chart renderer
│   ├── results_renderer.hpp        # Presentation reporting engine
│   ├── cli.hpp                     # Interactive CLI controller
│   ├── csv_parser.hpp              # RFC 4180 CSV parser
│   ├── workload_importer.hpp       # CSV workload ingestion & validation
│   └── csv_exporter.hpp            # CSV export engine & formula injection defense
│
├── src/                            # Implementation source files
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
│   ├── gantt_chart.cpp
│   ├── results_renderer.cpp
│   ├── cli.cpp
│   ├── cli_main.cpp                # CLI entry point (main)
│   ├── csv_parser.cpp
│   ├── workload_importer.cpp
│   └── csv_exporter.cpp
│
├── examples/                       # Example runners and sample workloads
│   ├── CMakeLists.txt
│   ├── basic_usage.cpp             # C++ API usage example
│   ├── sample_workload.csv         # Basic CSV workload
│   ├── sample_workload_with_priority.csv # Workload with priorities
│   └── sample_workload_mlq.csv     # Multilevel Queue workload
│
└── tests/                          # Automated GoogleTest test suite (601 tests)
    ├── CMakeLists.txt
    ├── test_process.cpp
    ├── test_process_validator.cpp
    ├── test_fcfs_scheduler.cpp
    ├── test_sjf_scheduler.cpp
    ├── test_srtf_scheduler.cpp
    ├── test_rr_scheduler.cpp
    ├── test_priority_scheduler.cpp
    ├── test_mlq_scheduler.cpp
    ├── test_mlfq_scheduler.cpp
    ├── test_scheduling_engine.cpp
    ├── test_cli.cpp
    ├── test_gantt_chart.cpp
    ├── test_results_renderer.cpp
    ├── test_csv_parser.cpp
    ├── test_csv_workload_importer.cpp
    ├── test_csv_results_exporter.cpp
    ├── test_cli_csv.cpp
    ├── test_scheduling_integration.cpp
    ├── test_pipeline_integration.cpp
    └── test_cli_integration.cpp
```

---

## Quickstart: Build and Run

### 1. Prerequisites
- Windows 10/11 (64-bit)
- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.20+
- vcpkg with `VCPKG_ROOT` environment variable configured

### 2. Configure & Build
Open **Developer PowerShell for VS 2022** in the project root:

```powershell
# Configure (Debug or Release)
cmake --preset windows-msvc-debug

# Build
cmake --build --preset debug

# Run all 601 tests
ctest --preset debug-test
```

### 3. Launch the Interactive CLI
```powershell
.\build\debug\Debug\cpu_scheduler_cli.exe
```

---

## Basic Usage Example

### Running an FCFS Simulation via the CLI
1. Launch `cpu_scheduler_cli.exe`.
2. Select `1` for **First Come, First Served (FCFS)**.
3. Enter process count `3`.
4. Enter process parameters:
   - Process 1: ID `P1`, Arrival `0`, Burst `5`
   - Process 2: ID `P2`, Arrival `2`, Burst `3`
   - Process 3: ID `P3`, Arrival `4`, Burst `1`

The application produces the complete simulation report:

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

Process Execution Details:
PID      Arrival   Burst  Completion  Turnaround   Waiting  Response
-----------------------------------------------------------------
P1             0       5           5           5         0         0
P2             2       3           8           6         3         3
P3             4       1           9           5         4         4
-----------------------------------------------------------------

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

Gantt Chart:
+-------+-----+---+
|  P1   | P2  |P3 |
+-------+-----+---+
0       5     8   9

Execution Timeline:
  [0 - 5: P1] [5 - 8: P2] [8 - 9: P3]
```

### Running with a CSV Workload
At the process count prompt, enter `0` and provide the file path:
```text
Enter number of processes (1-100) or 0 to import from CSV: 0
Enter CSV workload file path: examples/sample_workload.csv
```

---

## Detailed Documentation Links

- **[User Guide](docs/USER_GUIDE.md)**: Step-by-step instructions for installation, building, CLI navigation, manual process entry, algorithm configuration, CSV import/export, metrics interpretation, Gantt charts, error recovery, and troubleshooting.
- **[Algorithm Reference](docs/ALGORITHM_REFERENCE.md)**: Theoretical specifications, tie-breaking rules, configuration options, advantages, trade-offs, and step-by-step worked numerical examples for all eight algorithms.
- **[CSV Reference](docs/CSV_REFERENCE.md)**: Specifications for workload CSV schema, header aliases, RFC 4180 quoting, validation rules, results export formats, formula injection defense, Windows staged file replacement, and UTF-8 encoding requirements.
- **[Architectural Design](docs/ARCHITECTURE.md)**: Software design principles, component boundaries, data flow diagrams, data model invariants, scheduler implementations, and testing organization.

---

## License

This project is open-source educational software developed for operating system CPU scheduling simulation and analysis.
