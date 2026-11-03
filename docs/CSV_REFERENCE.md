# CPU Scheduling Simulator — CSV Format & Integration Reference

A comprehensive specification of CSV workload importation, schema validation, results exportation, security mitigation, and encoding requirements for the C++20 CPU Scheduling Simulator.

---

## Table of Contents

1. [Overview & Standards Compliance](#1-overview--standards-compliance)
2. [Workload CSV Schema](#2-workload-csv-schema)
   - [Supported Columns & Header Aliases](#supported-columns--header-aliases)
   - [Required vs. Optional Columns](#required-vs-optional-columns)
3. [RFC 4180 Quoting and Escaping Rules](#3-rfc-4180-quoting-and-escaping-rules)
4. [Validation Rules and Integrity Constraints](#4-validation-rules-and-integrity-constraints)
5. [Transactional Import & Error Diagnostics](#5-transactional-import--error-diagnostics)
6. [Verified Workload Examples](#6-verified-workload-examples)
7. [Results Export Formats](#7-results-export-formats)
   - [Process Execution Metrics CSV](#process-execution-metrics-csv)
   - [Execution Timeline CSV](#execution-timeline-csv)
8. [Formula Injection Defense (CSV Injection)](#8-formula-injection-defense-csv-injection)
9. [Safe Staged File Replacement (Windows)](#9-safe-staged-file-replacement-windows)
10. [Character Encoding & UTF-8 BOM Limitations](#10-character-encoding--utf-8-bom-limitations)

---

## 1. Overview & Standards Compliance

The simulator includes a dedicated, zero-dependency RFC 4180 compliant CSV engine:
- **Parser (`cpu_scheduler::CsvParser`)**: High-performance finite-state machine handling commas, quoted strings, escaped quotes (`""`), embedded commas, embedded newlines, and variable record lengths.
- **Importer (`cpu_scheduler::WorkloadImporter`)**: Validates and maps CSV rows into immutable `Process` workload models.
- **Exporter (`cpu_scheduler::CsvExporter`)**: Serializes simulation metrics and chronological execution timelines into spreadsheet-safe CSV documents.

---

## 2. Workload CSV Schema

A workload CSV file must begin with a single header row defining the column layout. Subsequent rows define individual process specifications.

### Supported Columns & Header Aliases
Headers are matched **case-insensitively** and tolerate leading and trailing whitespace:

| Canonical Column | Accepted Aliases | Data Type | Default Value | Description |
| :--- | :--- | :---: | :---: | :--- |
| `process_id` | `pid`, `process`, `id` | String | *Required* | Unique process identifier (e.g. `P1`, `JobA`). |
| `arrival_time` | `arrival`, `at` | Int64 | *Required* | Simulation arrival timestamp ($t \ge 0$). |
| `burst_time` | `burst`, `bt` | Int64 | *Required* | Required CPU execution time ($BT > 0$). |
| `priority` | `prio` | Int32 | `0` | Scheduling priority (smaller value = higher priority). |
| `queue_id` | `queue`, `qid` | String | `""` | Target queue identifier for Multilevel Queue (MLQ). |

### Required vs. Optional Columns
- **Required**: `process_id`, `arrival_time`, and `burst_time` must be present in every workload CSV.
- **Optional**: `priority` and `queue_id` may be omitted. If `priority` is omitted, processes default to priority `0`. If `queue_id` is omitted, the queue binding defaults to an empty string.
- **Unrecognized Columns**: Any column header not matching one of the recognized aliases causes the entire import to fail with a clear diagnostic message.
- **Duplicate Headers**: Declaring duplicate headers (e.g. having both `pid` and `process_id` in the same file) is rejected.

---

## 3. RFC 4180 Quoting and Escaping Rules

The parser follows standard RFC 4180 grammar:

1. **Unquoted Fields**: Fields containing only standard alphanumeric characters, spaces, hyphens, or periods do not require quotes:
   ```csv
   P1,0,5
   ```
2. **Quoted Fields**: Any field containing commas, double quotes, or newline characters must be enclosed in double quotes:
   ```csv
   "Task, Alpha",0,4
   ```
3. **Escaped Double Quotes**: To represent a literal double quote inside a quoted field, escape it by doubling the quote (`""`):
   ```csv
   "Process ""Beta""",1,3
   ```
4. **Line Endings**: Both Windows CRLF (`\r\n`) and Unix LF (`\n`) line endings are supported interchangeably.
5. **Trailing Newline**: Files with or without a final trailing newline are parsed cleanly.

---

## 4. Validation Rules and Integrity Constraints

Every workload CSV must satisfy strict simulation constraints:

- **Workload Size Limit**: The file must contain between **1 and 100 process records**. Workloads exceeding 100 records are rejected.
- **Non-Empty Process IDs**: Process IDs cannot be empty or consist solely of whitespace.
- **Unique Process IDs**: Every process ID must be unique. Uniqueness is evaluated **case-insensitively** with trimmed whitespace (`p1` conflicts with `P1`).
- **Non-Negative Arrival Time**: `arrival_time` must be an integer $\ge 0$.
- **Positive Burst Time**: `burst_time` must be an integer $> 0$.
- **Integer Range**: All integer fields must fit within standard 64-bit signed integer limits (`int64_t`) without overflow.
- **Field Count Consistency**: Every data row must contain the exact same number of fields as the header row.

---

## 5. Transactional Import & Error Diagnostics

Workload importation is strictly **all-or-nothing (transactional)**:
- If a CSV file contains syntax errors, schema errors, or row validation errors, **no processes are returned**, and the simulator state is left completely untouched.
- Rather than stopping at the first error, the importer collects and reports all validation failures with **1-based row and column coordinates**.

#### Example Error Output from CLI:
```text
Error: CSV import failed with 2 error(s):
  - Row 3: 'burst_time' must be a strictly positive integer (> 0) (got '0').
  - Row 4: Duplicate process ID 'P1' (conflicts with row 2).
Please correct the file or try again.
```

---

## 6. Verified Workload Examples

The following sample files are included in the repository's `examples/` directory and have been verified against the test suite:

### Example A: Basic Workload (`examples/sample_workload.csv`)
Used for FCFS, SJF, SRTF, and Round Robin:
```csv
process_id,arrival_time,burst_time
P1,0,5
P2,1,3
P3,2,8
P4,3,6
```

### Example B: Workload with Priorities (`examples/sample_workload_with_priority.csv`)
Used for Priority (Non-Preemptive and Preemptive). Notice that `P4` has priority `0` (highest priority):
```csv
process_id,arrival_time,burst_time,priority
P1,0,4,2
P2,1,5,1
P3,2,2,3
P4,3,1,0
```

### Example C: Multilevel Queue Workload (`examples/sample_workload_mlq.csv`)
Specifies explicit queue bindings for MLQ:
```csv
process_id,arrival_time,burst_time,priority,queue_id
P1,0,4,1,Q_Interactive
P2,0,6,2,Q_Interactive
P3,2,8,0,Q_Batch
P4,4,3,1,Q_Batch
```

---

## 7. Results Export Formats

The simulator exports two complementary CSV document types for analysis in spreadsheet applications (Microsoft Excel, Google Sheets, LibreOffice Calc) or data science tools (Python pandas, R):

### Process Execution Metrics CSV
Serializes individual process lifecycle statistics:

```csv
process_id,arrival_time,burst_time,completion_time,turnaround_time,waiting_time,response_time,priority,queue_id
P1,0,4,4,4,0,0,2,Q_Interactive
P2,0,6,10,10,4,4,1,Q_Interactive
P3,2,8,18,16,8,8,0,Q_Batch
P4,4,3,21,17,14,14,1,Q_Batch
```

#### Columns:
1. `process_id`: Process identifier.
2. `arrival_time`: Timestamp of process arrival.
3. `burst_time`: Total CPU burst time.
4. `completion_time`: Timestamp when process finished execution.
5. `turnaround_time`: Elapsed time from arrival to completion ($CT - AT$).
6. `waiting_time`: Time spent waiting in ready queues ($TAT - BT$).
7. `response_time`: Time from arrival to initial execution slice ($FirstStart - AT$).
8. `priority`: Assigned priority integer.
9. `queue_id`: Assigned queue identifier (or empty string if not applicable).

### Execution Timeline CSV
Serializes the chronological CPU execution history, including explicit idle intervals:

```csv
start_time,end_time,process_id,queue_id
0,4,P1,Q_Interactive
4,10,P2,Q_Interactive
10,18,P3,Q_Batch
18,21,P4,Q_Batch
```

#### Columns:
1. `start_time`: Beginning timestamp of the slice.
2. `end_time`: Ending timestamp of the slice.
3. `process_id`: ID of the executing process, or `IDLE` during CPU idle periods.
4. `queue_id`: Queue ID associated with the process (or empty string during idle).

---

## 8. Formula Injection Defense (CSV Injection)

Spreadsheet applications (such as Microsoft Excel and LibreOffice Calc) automatically interpret cells starting with `=`, `+`, `-`, or `@` as executable formulas or DDE macro commands. Malicious or unusual process IDs (such as `=cmd|' /C calc'!A0` or `-10`) could execute unintended commands when opened in a spreadsheet.

To eliminate this vulnerability:
- **Export Boundary Neutralization**: During CSV export, `CsvExporter` inspects text columns (`process_id`, `queue_id`). If a string begins with `=`, `+`, `-`, or `@`, the exporter prepends a single quote apostrophe (`'`):
  ```csv
  "'=cmd|' /C calc'!A0",0,3,3,3,0,0,0,
  ```
- **In-Memory Immutability**: The in-memory `SchedulingResult` data structures remain completely unmodified.
- **Numeric Preservation**: Numeric fields (such as negative priorities like `-5`) are **not** escaped because spreadsheet software safely parses them as numbers, preserving standard arithmetic formulas and statistical charting.

---

## 9. Safe Staged File Replacement (Windows)

When exporting results to disk, the exporter guarantees file integrity:

1. **Staged Temporary Writing**: Output is written to a temporary sibling file with the extension `.tmp` (e.g. `results.csv.tmp`).
2. **Atomic In-Place Replacement**: On Windows, the simulator calls the Win32 API function `ReplaceFileW`:
   - If the destination file already exists and overwrite is approved, `ReplaceFileW` replaces the destination in place without deleting it first.
   - If writing fails (e.g., disk full, permission denied), the temporary file is deleted, and the **original destination file remains completely intact**.
3. **Overwrite Rejection**: If the destination file exists and the user declines to overwrite, the operation aborts cleanly without altering the existing file.

---

## 10. Character Encoding & UTF-8 BOM Limitations

### Required Encoding
The simulator expects all CSV files to be encoded in **UTF-8 without Byte Order Mark (BOM)**:
- Standard ASCII and UTF-8 characters are supported.
- If your text editor saves files with a **UTF-8 BOM** (`0xEF, 0xBB, 0xBF`), the three preamble bytes remain attached to the first column header (`\xEF\xBB\xBFprocess_id`). Because the simulator strictly matches column headers, this causes an error:
  ```text
  Error: Missing required column 'process_id'.
  ```

### How to Save in UTF-8 without BOM:
- **Visual Studio Code**: In the status bar at the bottom right, verify encoding is set to `UTF-8` (not `UTF-8 with BOM`).
- **Notepad (Windows 10/11)**: Click **Save As**, and ensure the **Encoding** dropdown is set to `UTF-8`.
- **PowerShell**: Use `Set-Content -Encoding utf8NoBOM` when scripting CSV generation.
