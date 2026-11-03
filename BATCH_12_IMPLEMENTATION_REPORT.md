# CPU Scheduling Simulator — Batch 12: CSV Workload Import and Results Export Implementation Report

## 1. Summary of Implementation

Batch 12 delivers production-grade CSV workload import and scheduling result export capabilities to the C++20 CPU Scheduling Simulator.

Key features implemented:
- **RFC 4180 Compliant CSV Parser (`cpu_scheduler::CsvParser`)**: High-performance, fully standard CSV engine supporting comma delimiters, header rows, quoted fields, embedded commas, escaped double quotes (`""`), multi-line quoted fields, CRLF and LF line endings, empty fields, files without trailing newlines, and detection of malformed quoting syntax.
- **Transactional Workload Importer (`cpu_scheduler::WorkloadImporter`)**: Ingests and validates process workloads from CSV formats. Supports required columns (`process_id`, `arrival_time`, `burst_time`) and optional columns (`priority`, `queue_id`), case-insensitive headers, trimmed whitespace, all-or-nothing validation, duplicate process ID rejection, safe integer conversion with overflow protection, 100-record workload limit enforcement, and detailed 1-based row diagnostics.
- **Results CSV Exporter (`cpu_scheduler::CsvExporter`)**: Serializes simulation results into spreadsheet-compatible CSV documents:
  - **Process Execution Metrics CSV**: `process_id,arrival_time,burst_time,completion_time,turnaround_time,waiting_time,response_time,priority,queue_id`.
  - **Execution Timeline CSV**: `start_time,end_time,process_id,queue_id` with explicit `IDLE` intervals and preserved chronological segment ordering.
- **Formula Injection Mitigation (CSV Injection)**: Proactively neutralizes potential spreadsheet formula execution characters (`=`, `+`, `-`, `@`) in text fields (`process_id`, `queue_id`) via single-quote apostrophe escaping at export boundaries. Numeric columns (`arrival_time`, `burst_time`, `priority`, etc.) remain raw numbers, and original in-memory `SchedulingResult` data structures are strictly unmodified.
- **Safe Atomic File Operations**: Writes export data to a temporary file (`.tmp`) before atomic renaming, preventing corrupted or partially written files upon disk or I/O failure, and supports overwrite protection.
- **Interactive CLI Integration (`cpu_scheduler::CliController`)**:
  - Main workflow enables choosing between manual workload entry and CSV file import (`Enter number of processes (1-100) or 0 to import from CSV`).
  - Comprehensive error recovery and retry upon invalid CSV paths or schema errors.
  - Post-simulation menu provides CSV export options (process metrics, timeline, both, or skip) with user-configurable destination paths.
- **Sample Workload Files**: Provided valid sample CSV files in `examples/`: `sample_workload.csv`, `sample_workload_with_priority.csv`, and `sample_workload_mlq.csv`.
- **Zero Compiler Warnings**: Builds with zero warnings under MSVC `/W4 /permissive-` in both Debug and Release presets.
- **Automated Test Suite**: 39 new comprehensive GoogleTest tests added (11 for `CsvParser`, 17 for `WorkloadImporter`, 8 for `CsvExporter`, 3 for `CliCsv`), bringing total repository test count to **571 tests** (100% passing in both Debug and Release).

---

## 2. Files Added and Modified

### Added Files:
- `include/cpu_scheduler/csv_parser.hpp`: Header declaring `CsvParser`, `CsvRow`, `CsvParseError`, and `CsvParseResult`.
- `src/csv_parser.cpp`: RFC 4180 compliant CSV parser implementation.
- `include/cpu_scheduler/workload_importer.hpp`: Header declaring `WorkloadImporter`, `WorkloadImportResult`, and `WorkloadImportError`.
- `src/workload_importer.cpp`: CSV workload ingestion and row diagnostic validation.
- `include/cpu_scheduler/csv_exporter.hpp`: Header declaring `CsvExporter`, `CsvExportOptions`, and `CsvExportResult`.
- `src/csv_exporter.cpp`: Process metrics and timeline CSV serialization with formula injection protection and atomic file writing.
- `examples/sample_workload.csv`: Sample basic workload CSV without priorities.
- `examples/sample_workload_with_priority.csv`: Sample workload CSV with process priorities.
- `examples/sample_workload_mlq.csv`: Sample workload CSV with Multilevel Queue assignments.
- `tests/test_csv_parser.cpp`: Unit tests for CSV parser (11 tests).
- `tests/test_csv_workload_importer.cpp`: Unit tests for workload importer (17 tests).
- `tests/test_csv_results_exporter.cpp`: Unit tests for results exporter and formula injection (8 tests).
- `tests/test_cli_csv.cpp`: Integration tests for CLI CSV workflows (3 tests).
- `BATCH_12_IMPLEMENTATION_REPORT.md`: This comprehensive implementation report.

### Modified Files:
- `include/cpu_scheduler/cli.hpp`: Declared `format_csv_export_menu`, `prompt_csv_workload`, and `prompt_csv_export_menu`.
- `src/cli.cpp`: Integrated CSV workload import option and CSV export menu into `CliController`.
- `CMakeLists.txt`: Added `csv_parser.cpp`, `workload_importer.cpp`, and `csv_exporter.cpp` to `cpu_scheduler_lib`.
- `tests/CMakeLists.txt`: Added `test_csv_parser.cpp`, `test_csv_workload_importer.cpp`, `test_csv_results_exporter.cpp`, and `test_cli_csv.cpp` to `cpu_scheduler_tests`.
- `README.md`: Updated project status, test count (571 tests), CSV import/export documentation, schema, and file tree.
- `docs/ARCHITECTURE.md`: Documented `CsvParser` (Section 2.17), `WorkloadImporter` (Section 2.18), and `CsvExporter` (Section 2.19).

---

## 3. CSV Parser Architecture

`cpu_scheduler::CsvParser` implements an RFC 4180 finite-state automaton that iterates through the input stream:
- **States**: `FieldStart`, `UnquotedField`, `QuotedField`, `AfterClosingQuote`.
- **Quoted Fields**: When a field starts with `"`, it transitions to `QuotedField` and preserves all characters (including commas and newlines) until a matching quote is encountered.
- **Escaped Quotes**: Double quotes (`""`) inside quoted fields are unescaped to a single quote (`"`).
- **Syntax Error Detection**:
  - Unclosed quote at EOF reports `Unclosed quote at end of input` with exact line and column numbers.
  - Characters occurring after a closing quote before a delimiter (e.g. `"val"xyz,`) are detected and flagged.
  - Unexpected quotes inside unquoted fields are flagged as syntax errors.
- **Line Endings & Trailing Records**: Uniformly handles CRLF (`\r\n`), LF (`\n`), and final records lacking trailing newlines.

---

## 4. Workload CSV Schema & Validation

### Header Schema:
- **Required Columns**:
  - `process_id` (aliases: `pid`, `process`, `id`)
  - `arrival_time` (aliases: `arrival`, `at`)
  - `burst_time` (aliases: `burst`, `bt`)
- **Optional Columns**:
  - `priority` (aliases: `prio`, default 0)
  - `queue_id` (aliases: `queue`, `qid`)
- **Header Rules**:
  - Matched case-insensitively (`Process_ID` == `process_id`).
  - Harmless surrounding whitespace is trimmed.
  - Duplicate column headers (e.g. `pid,process_id`) are rejected.
  - Unsupported columns (e.g. `extra_col`) are rejected with clear diagnostics.

### Row-Level Validation & Error Diagnostics:
- Maximum workload count: bounded to 100 processes (exceeding limit rejected).
- Process ID: non-empty, non-whitespace, case-insensitive uniqueness check (`p1` conflicts with `P1`).
- Arrival Time: non-negative integer (`>= 0`), checked via `std::from_chars` to prevent overflow.
- Burst Time: strictly positive integer (`> 0`), checked via `std::from_chars`.
- Priority: signed integer (`int`), defaults to 0 if column absent or cell empty.
- Queue ID: trimmed string identifier.
- Field count on every data row must match the header column count.
- **All-or-Nothing Semantics**: If any row has errors, the entire import fails, returning all accumulated diagnostics with 1-based row numbers.

---

## 5. Results & Timeline CSV Export Schema

### Process Metrics CSV:
```csv
process_id,arrival_time,burst_time,completion_time,turnaround_time,waiting_time,response_time,priority,queue_id
P1,0,4,4,4,0,0,1,Q_High
P2,2,3,7,5,2,2,2,Q_Low
```
- Numeric fields are formatted without modification.
- Optional fields (`queue_id`) are empty string when not applicable.

### Timeline CSV:
```csv
start_time,end_time,process_id,queue_id
0,2,IDLE,
2,6,P1,Q_High
6,9,P2,Q_Low
```
- Idle intervals are labeled explicitly as `IDLE`.
- Original segment sequence and repeated slices (from preemption or Round Robin) are preserved faithfully.

---

## 6. Formula Injection Mitigation (CSV Injection)

When opening CSV files in spreadsheet applications (e.g., Microsoft Excel, Google Sheets, LibreOffice Calc), cells beginning with `=`, `+`, `-`, or `@` can be executed as active dynamic formulas.

`CsvExporter::sanitize_formula()` mitigates this vulnerability:
- **Mitigation Strategy**: If a text field starts with `=`, `+`, `-`, `@`, `\t`, or `\r`, it is prepended with a single quote apostrophe (`'`).
  - Example: `=cmd|' /C calc'!A0` $\rightarrow$ `'=cmd|' /C calc'!A0`
  - Example: `-P1` $\rightarrow$ `'-P1`
  - Enclosed in CSV quotes: `"-P1"`
- **Numeric Field Preservation**: Numeric fields (`arrival_time`, `burst_time`, `priority`, etc.) are NEVER treated as untrusted text. A negative priority integer `-5` is exported as `-5`, not `'-5`.
- **In-Memory Immutability**: The original in-memory `SchedulingResult` data structures are completely unmodified; neutralization occurs strictly at the CSV export boundary.

---

## 7. Interactive CLI Integration

- **Workload Choice**:
  ```text
  --- Workload Configuration ---
  Enter number of processes (1-100) or 0 to import from CSV:
  ```
  - Entering `1-100` proceeds with manual entry (preserving 100% backward compatibility with existing tests).
  - Entering `0` opens the CSV import prompt, loading from the specified file path and reporting clear row diagnostics if errors exist.
- **Export Menu**:
  ```text
  ------------------------------------------------------------
  Options:
    1. Run another simulation
    2. Exit
    3. Export results to CSV
  ------------------------------------------------------------
  Enter selection (1-3):
  ```
  - Entering `2` exits cleanly (preserving existing tests).
  - Entering `3` displays CSV export options (process metrics, timeline, both, or return) with destination path selection and overwrite confirmation.

---

## 8. Build and Test Verification

### Debug Configuration (`windows-msvc-debug`):
```text
cmake --build --preset debug
MSBuild version 17.14.60+43b635718 for .NET Framework
  cpu_scheduler_lib.vcxproj -> Debug\cpu_scheduler_lib.lib
  cpu_scheduler_cli.vcxproj -> Debug\cpu_scheduler_cli.exe
  cpu_scheduler_tests.vcxproj -> tests\Debug\cpu_scheduler_tests.exe
Build succeeded.
    0 Warning(s)
    0 Error(s)
```

```text
ctest --preset debug-test --output-on-failure
100% tests passed out of 571
Total Test time (real) = 22.77 sec
```

### Release Configuration (`windows-msvc-release`):
```text
cmake --build --preset release
MSBuild version 17.14.60+43b635718 for .NET Framework
  cpu_scheduler_lib.vcxproj -> Release\cpu_scheduler_lib.lib
  cpu_scheduler_cli.vcxproj -> Release\cpu_scheduler_cli.exe
  cpu_scheduler_tests.vcxproj -> tests\Release\cpu_scheduler_tests.exe
Build succeeded.
    0 Warning(s)
    0 Error(s)
```

```text
ctest --preset release-test --output-on-failure
100% tests passed out of 571
Total Test time (real) = 15.53 sec
```

---

## 9. Baseline & Git Integrity

- **Base Commit (Batch 11)**: `3c84133778288849b5f73f62d650165ab2428ddb`
- **Parent Commit (Batch 10)**: `c8a0eba98e012d5eae2217c95ddcd6d45fd7ea6a`
- **Active Branch**: `main`
- **Total Tests**: **571** (532 baseline tests preserved + 39 new tests, 100% passing).
- **Audit Reports Preserved**: `BATCH_5_INDEPENDENT_AUDIT_REPORT.md` through `BATCH_11_INDEPENDENT_AUDIT_REPORT.md` intact and untracked.
- **Working Tree Cleanliness**: Line endings normalized to CRLF, zero whitespace errors on `git diff --check`.
