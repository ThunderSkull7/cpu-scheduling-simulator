# CPU Scheduling Simulator — Release Readiness Assessment

**Milestone**: Batch 15 — Final Audit and Release Preparation
**Target**: Local Release Candidate (v1.0-RC)
**Date**: November 3, 2026
**Audience**: Maintainers, Evaluators, and End Users
**Readiness Status**: **READY FOR RELEASE**

---

## 1. Project Overview

The **CPU Scheduling Simulator** is a production-grade, educational and research-oriented C++20 desktop simulator for standard Operating System CPU scheduling algorithms.

The simulator provides:
- Deterministic event-driven and tick-based execution tracking.
- Eight scheduling algorithms spanning non-preemptive, preemptive, priority, and multi-queue paradigms.
- A unified scheduling engine decoupling presentation and I/O from scheduler algorithms.
- An interactive terminal command-line interface (CLI) with robust input validation and error recovery.
- Duration-aware text Gantt chart generation with proportional scaling, timestamp alignment, and multi-row wrapping.
- An RFC 4180 compliant CSV parser, workload importer, and spreadsheet-compatible results exporter with formula injection defense.
- A 601-test automated regression suite with zero compiler warnings under MSVC `/W4 /permissive-`.

---

## 2. Implemented Feature Inventory

| Subsystem | Implemented Components | Primary Source Files | Unit & Integration Test Coverage |
| :--- | :--- | :--- | :--- |
| **Core Process Model** | `Process`, `ProcessValidator` | `process.cpp`, `process_validator.cpp` | `test_process.cpp` (7), `test_process_validator.cpp` (16) |
| **Simulation State & Timeline** | `ProcessState`, `ExecutionSegment`, `SchedulingResult` | `process_state.cpp`, `execution_segment.cpp`, `scheduling_result.cpp` | Integrated across all scheduler tests |
| **FCFS Engine** | `FcfsScheduler` | `fcfs_scheduler.cpp` | `test_fcfs_scheduler.cpp` (34) |
| **SJF Engine (Non-Preemptive)** | `SjfScheduler` | `sjf_scheduler.cpp` | `test_sjf_scheduler.cpp` (52) |
| **SRTF Engine (Preemptive)** | `SrtfScheduler` | `srtf_scheduler.cpp` | `test_srtf_scheduler.cpp` (63) |
| **Round Robin Engine** | `RrScheduler` | `rr_scheduler.cpp` | `test_rr_scheduler.cpp` (54) |
| **Priority Engines (NP & P)** | `NonPreemptivePriorityScheduler`, `PreemptivePriorityScheduler` | `priority_scheduler.cpp` | `test_priority_scheduler.cpp` (68) |
| **Multilevel Queue (MLQ)** | `MlqScheduler`, `MlqWorkload`, `MlqQueueConfig` | `mlq_scheduler.cpp` | `test_mlq_scheduler.cpp` (77) |
| **Multilevel Feedback Queue** | `MlfqScheduler`, `MlfqConfig` | `mlfq_scheduler.cpp` | `test_mlfq_scheduler.cpp` (77) |
| **Unified Dispatch Engine** | `SchedulingEngine`, `SchedulingRequest` | `scheduling_engine.cpp` | `test_scheduling_engine.cpp` (39) |
| **Interactive Terminal CLI** | `CliController`, `cli_main` | `cli.cpp`, `cli_main.cpp` | `test_cli.cpp` (24), `test_cli_csv.cpp` (3), `test_cli_integration.cpp` (9) |
| **Gantt Chart Visualization** | `GanttChart` | `gantt_chart.cpp` | `test_gantt_chart.cpp` (13) |
| **Results Presentation** | `ResultsRenderer` | `results_renderer.cpp` | `test_results_renderer.cpp` (8) |
| **RFC 4180 CSV Engine** | `CsvParser` | `csv_parser.cpp` | `test_csv_parser.cpp` (11) |
| **CSV Workload Importer** | `WorkloadImporter` | `workload_importer.cpp` | `test_csv_workload_importer.cpp` (17) |
| **CSV Results Exporter** | `CsvExporter` | `csv_exporter.cpp` | `test_csv_results_exporter.cpp` (8) |
| **Integration & Invariants** | Universal Invariant Checker, Full Pipeline | `test_scheduling_integration.cpp`, `test_pipeline_integration.cpp` | `test_scheduling_integration.cpp` (8), `test_pipeline_integration.cpp` (7) |
| **Total Automated Tests** | **Full Regression Suite** | **20 Test Source Files** | **601 Tests (100% Passing)** |

---

## 3. Prerequisites & Supported Environment

- **Operating System**: Windows 10 or Windows 11 (64-bit x64).
- **C++ Compiler**: Microsoft Visual C++ (MSVC) from Visual Studio 2022 (version 17.0+).
- **C++ Standard**: ISO C++20 (`/std:c++20 /permissive- /W4`).
- **Build System**: CMake 3.20+ with `CMakePresets.json`.
- **Package Manager**: vcpkg in manifest mode (`vcpkg.json`). Requires `VCPKG_ROOT` environment variable.
- **Terminal**: Windows Terminal, PowerShell 7+, or Command Prompt. For proper display of Unicode box-drawing characters, set console to UTF-8 via `chcp 65001`.

---

## 4. Exact Build and Test Commands

All operations are executed via standard CMake Presets:

```powershell
# 1. Configure Presets:
cmake --preset windows-msvc-debug
cmake --preset windows-msvc-release

# 2. Build Presets:
cmake --build --preset debug
cmake --build --preset release

# 3. Test Presets:
ctest --preset debug-test
ctest --preset release-test
```

### Verified Build Artifacts
- CLI Executable: `build\<preset>\Debug\cpu_scheduler_cli.exe` (or `Release\cpu_scheduler_cli.exe`)
- Automated Test Suite: `build\<preset>\tests\Debug\cpu_scheduler_tests.exe`
- API Usage Example: `build\<preset>\examples\Debug\basic_usage.exe`

---

## 5. How to Run the Application

Launch the interactive CLI directly:

```powershell
# Debug build:
.\build\debug\Debug\cpu_scheduler_cli.exe

# Release build:
.\build\release\Release\cpu_scheduler_cli.exe
```

### Basic Interactive Workflow:
1. Choose an algorithm from the main menu (`1` to `8`).
2. Enter the process count (`1` to `100`), or enter `0` to import from a CSV file.
3. Configure algorithm parameters if applicable (e.g. quantum for Round Robin).
4. View the Simulation Summary, Process Table, Aggregate Metrics, Gantt Chart, and Execution Timeline.
5. Choose option `3` from the post-simulation menu to export results to CSV.

---

## 6. Documentation and Sample Data References

Comprehensive documentation is provided in the repository:

- **[README.md](README.md)**: Main landing page, project overview, quickstart, and file tree.
- **[docs/USER_GUIDE.md](docs/USER_GUIDE.md)**: Detailed step-by-step user guide covering installation, CLI navigation, algorithm configuration, metrics, Gantt chart interpretation, and error recovery.
- **[docs/ALGORITHM_REFERENCE.md](docs/ALGORITHM_REFERENCE.md)**: Theoretical and practical specifications of all eight algorithms with step-by-step worked numerical examples and comparison matrices.
- **[docs/CSV_REFERENCE.md](docs/CSV_REFERENCE.md)**: Workload schema, header aliases, quoting rules, results export formats, formula injection defense, Windows staged file replacement, and encoding rules.
- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)**: System architectural design, component boundaries, data flow diagrams, and test suite organization.

### Sample CSV Workloads
Verified sample workloads are available in `examples/`:
- `examples/sample_workload.csv`: Standard workload without priorities (FCFS, SJF, SRTF, RR).
- `examples/sample_workload_with_priority.csv`: Workload with process priorities.
- `examples/sample_workload_mlq.csv`: Workload with queue assignments for MLQ.

---

## 7. Verified Test Results

| Build Configuration | Total Tests | Passed | Failed | Skipped | Pass Rate | Execution Time | Compiler Warnings |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Debug (`debug-test`)** | 601 | 601 | 0 | 0 | **100%** | 18.48 s | 0 (`/W4 /permissive-`) |
| **Release (`release-test`)** | 601 | 601 | 0 | 0 | **100%** | 17.06 s | 0 (`/W4 /permissive-`) |

---

## 8. Known Implementation Boundaries & Disclosures

1. **MLFQ Final Queue Level Metadata**:
   - `ProcessState::queue_id` records each process's *final* queue level upon completion.
   - Individual execution segments in `ExecutionSegment` record contiguous CPU intervals without historical per-slice queue level transitions. MLFQ Gantt charts display process IDs without misleading slice-level queue labels.
2. **CSV UTF-8 Without BOM**:
   - CSV files must be encoded in UTF-8 without Byte Order Mark (BOM). Files saved with a UTF-8 BOM (`0xEF, 0xBB, 0xBF`) attach preamble bytes to the first column name, resulting in header rejection.
3. **Windows Unicode Console Encoding**:
   - Unicode box-drawing Gantt chart characters require Windows console code page 65001 (`chcp 65001`). ASCII border styling (`+`, `-`, `|`) is standard and universally portable.
4. **Formula Injection Mitigation**:
   - Text fields beginning with `=`, `+`, `-`, or `@` are neutralized with a leading single quote (`'`) upon CSV export. In-memory data structures and numeric fields remain unmutated.
5. **Safe Atomic Staged Replacement**:
   - On Windows, CSV file export stages output to `.tmp` files and calls `ReplaceFileW`. Original destination files are preserved untouched if writing fails.
6. **Workload Bounding**:
   - Workload size is bounded to a maximum of 100 processes per run to ensure interactive terminal responsiveness.

---

## 9. Release Checklist

- [x] All 8 scheduling algorithms fully implemented and verified against universal invariants.
- [x] Unified scheduling engine dispatches all algorithms with type safety.
- [x] Interactive CLI supports manual input, CSV import, configuration, and result reporting.
- [x] Gantt chart supports proportional block scaling, timestamp alignment, and multi-row wrapping.
- [x] CSV engine conforms to RFC 4180 with transactional import diagnostics.
- [x] CSV exporter implements spreadsheet formula injection defense.
- [x] Windows staged atomic file replacement via `ReplaceFileW` verified.
- [x] Zero compiler warnings under MSVC `/W4 /permissive-` in Debug and Release.
- [x] All 601 automated unit, integration, and regression tests passing 100%.
- [x] Comprehensive documentation (`README.md`, `USER_GUIDE.md`, `ALGORITHM_REFERENCE.md`, `CSV_REFERENCE.md`, `ARCHITECTURE.md`) complete and verified.
- [x] Documentation inconsistency (17 vs 20 test files in `docs/ARCHITECTURE.md`) resolved.
- [x] Historical audit reports preserved untouched.

---

## 10. Final Readiness Status

**VERDICT: READY FOR RELEASE**

The CPU Scheduling Simulator codebase meets all functional, architectural, performance, and documentation requirements. The project is verified as a release-ready local Release Candidate.
