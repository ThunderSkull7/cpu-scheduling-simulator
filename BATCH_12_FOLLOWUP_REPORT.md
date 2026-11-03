# Batch 12 — Follow-up Report: Git Integrity and Safe CSV File Replacement

**Target**: Batch 12 Follow-up
**Date**: 2026-11-03
**Status**: Completed & Verified

---

## 1. Explanation of the Two Commit Hashes (Part A)

During the independent audit, a discrepancy was noted between two 40-character commit hashes associated with the Batch 12 commit:
- Hash A (reported in implementation summary prompt): `cf0aca7ed73dbb1086c8d7e008ba9cfeb0ca41bb`
- Hash B (verified by Git repository inspection): `cf0aca7c52553a6276155ff413880fc74e368ed0`

### Investigation Details
1. **Git Object Database Check**:
   - `git cat-file -t cf0aca7c52553a6276155ff413880fc74e368ed0` returned `commit`.
   - `git cat-file -t cf0aca7ed73dbb1086c8d7e008ba9cfeb0ca41bb` returned `fatal: git cat-file: could not get object info`.
   - Hash A does **not exist** anywhere in the Git object database, reflog, or history.
2. **Commit History & Lineage**:
   - `git reflog` confirms that only a single commit was created on branch `main` for Batch 12:
     `HEAD@{0}: commit: feat: add CSV workload import and results export` with short hash `cf0aca7`.
   - The full commit hash produced by Git upon creating this commit was:
     `cf0aca7c52553a6276155ff413880fc74e368ed0`.
3. **Root Cause of Discrepancy**:
   - The implementation report (`BATCH_12_IMPLEMENTATION_REPORT.md`) was written and staged *prior* to creating the commit and recorded only its baseline parent (`3c84133778288849b5f73f62d650165ab2428ddb`).
   - When the assistant generated the conversational summary after committing, it noted the correct 7-character prefix `cf0aca7`, but the remaining 33 hexadecimal characters (`...ed73dbb1086c8d7e008ba9cfeb0ca41bb`) were an accidental reporting typo / synthetic hallucination.
   - **Conclusion**: There is no duplicate, amended, or orphaned commit. The genuine, verified Git identity of the Batch 12 commit has always been `cf0aca7c52553a6276155ff413880fc74e368ed0`.

---

## 2. Verified Original Batch 12 Commit Identity

- **Branch**: `main`
- **Full Hash**: `cf0aca7c52553a6276155ff413880fc74e368ed0`
- **Short Hash**: `cf0aca7`
- **Parent Hash**: `3c84133778288849b5f73f62d650165ab2428ddb` (Batch 11 approved baseline)
- **Subject**: `feat: add CSV workload import and results export`
- **Author/Committer Date**: `Tue Nov 3 10:09:19 2026 +0530`

---

## 3. Root Cause of the Overwrite-Safety Issue (Part B)

In the initial Batch 12 implementation, `export_process_metrics_to_file` and `export_timeline_to_file` in `src/csv_exporter.cpp` performed staged writing using the following sequence:
1. Write CSV contents to temporary file `path + ".tmp"`.
2. If `path` already exists, call `std::filesystem::remove(path, ec)`.
3. Call `std::filesystem::rename(temp_path, path, ec)`.

### Hazard
Deleting the existing destination file *before* renaming the temporary file introduced a vulnerability:
- If `rename()` failed (e.g. file lock, sharing violation, permission refusal, antivirus scanner interception, or power loss), the existing destination file was already permanently deleted.
- This violated transactional safety and created a data-loss window.

---

## 4. Exact Replacement Strategy & Limitations

### 4.1 Implementation Architecture
A dedicated helper function `commit_staged_file()` was introduced in `src/csv_exporter.cpp`:
1. **Never Delete Preemptively**: The destination file is never deleted prior to attempting replacement.
2. **Existing Destination (Overwrite Enabled)**:
   - On Windows, uses the Win32 `ReplaceFileW()` API (`dest_path.c_str(), temp_path.c_str(), nullptr, 0, nullptr, nullptr`).
   - `ReplaceFileW()` atomically replaces the file contents on NTFS in place while preserving file identity, attributes, and security descriptors.
   - If `ReplaceFileW()` fails (e.g. destination is read-only, locked, or permissions denied):
     - The existing destination file remains **100% untouched and preserved**.
     - The temporary file (`.tmp`) is cleaned up.
     - A formatted Win32 error message (`format_win32_error`) is returned to the caller.
   - If `ReplaceFileW()` reports `ERROR_FILE_NOT_FOUND` (indicating the file was deleted in a race after the exists check), it seamlessly falls back to `MoveFileExW()`.
3. **Non-Existent Destination**:
   - Uses `MoveFileExW(temp_path.c_str(), dest_path.c_str(), flags)`.
   - If `overwrite == false`, `flags` does **not** specify `MOVEFILE_REPLACE_EXISTING`. If another process creates the file in a race, `MoveFileExW` fails with `ERROR_ALREADY_EXISTS`, preventing silent accidental overwrite.
4. **Temporary File Cleanup**:
   - In all failure branches (stream write errors, destination exists with `overwrite=false`, replacement failure), the temporary `.tmp` file is immediately deleted to prevent disk clutter.

### 4.2 Limitations & Non-Claims
- **Filesystem Dependency**: `ReplaceFileW()` guarantees transactional atomic replacement on local NTFS filesystems. On remote SMB shares or FAT32 filesystems, the operation provides best-effort transactional safety as supported by the underlying filesystem driver.
- **Cross-Platform Fallback**: On non-Windows platforms (`#else`), `std::filesystem::rename` is used, which is POSIX atomic on same-filesystem volumes.

---

## 5. Files Changed

| File Path | Description of Changes |
| :--- | :--- |
| `include/cpu_scheduler/csv_exporter.hpp` | Updated class and method docstrings to reflect safe transactional replacement and accurate atomicity guarantees. |
| `src/csv_exporter.cpp` | Added Win32 `ReplaceFileW` and `MoveFileExW` integration via `commit_staged_file()`, Win32 error message formatting, eliminated preemptive deletion, and unified staged file commit across process metrics and timeline exports. |
| `tests/test_csv_results_exporter.cpp` | Added 6 comprehensive unit tests verifying content preservation, safe replacement, error reporting, and temporary file cleanup. |
| `README.md` | Refined terminology from unconditional "atomic file writing" to "safe staged file replacement (via ReplaceFileW on Windows)". |
| `docs/ARCHITECTURE.md` | Refined Section 2.19 to document safe transactional in-place replacement and failure recovery. |

---

## 6. Targeted Tests Added (Part C)

Six new automated tests were added to `tests/test_csv_results_exporter.cpp`:

1. `CsvExporterTest.FileExportToNewDestinationVerifiesContentAndNoTempLeftover`:
   - Exports process metrics to a brand-new file.
   - Verifies file contains expected CSV header and data, and that `.tmp` is cleaned up.
2. `CsvExporterTest.FileExportOverwriteDisabledPreservesOriginalFileContent`:
   - Pre-creates destination with critical user data and exports with `overwrite = false`.
   - Verifies export fails, returns appropriate diagnostic, preserves original file content unchanged, and deletes `.tmp`.
3. `CsvExporterTest.FileExportOverwriteEnabledReplacesContentCleanly`:
   - Pre-creates destination with stale data and exports with `overwrite = true`.
   - Verifies export succeeds, stale data is replaced with new CSV data, and no `.tmp` remains.
4. `CsvExporterTest.FileExportReplacementFailurePreservesDestinationAndCleansUpTemp`:
   - Pre-creates destination with critical data and marks it `FILE_ATTRIBUTE_READONLY`.
   - Calls export with `overwrite = true`.
   - Verifies `ReplaceFileW` fails, returns descriptive error message, strictly preserves destination data, and cleans up `.tmp`.
5. `CsvExporterTest.TimelineFileExportSafeReplacementAndContentVerification`:
   - Verifies safe overwrite replacement for chronological timeline CSV exports, ensuring old content is replaced and no `.tmp` remains.
6. `CsvExporterTest.TimelineFileExportReplacementFailurePreservesDestination`:
   - Verifies that timeline export failure against a read-only destination strictly preserves original destination content and cleans up `.tmp`.

---

## 7. Test and Build Results (Part D)

### 7.1 Regression and Test Matrix
- **Baseline Prior to Follow-up**: 571 tests.
- **New Tests Added**: 6 tests.
- **Final Test Total**: **577 tests**.

| Configuration | Test Command | Tests Run | Passed | Failed | Pass Rate | Time |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Debug** | `ctest --preset debug-test` | 577 | 577 | 0 | **100%** | 20.19s |
| **Release** | `ctest --preset release-test` | 577 | 577 | 0 | **100%** | 18.92s |

### 7.2 Compiler Diagnostics
- Both presets compiled cleanly with **0 compiler warnings** under MSVC `/W4 /permissive-`.
- Zero regressions across existing CSV parser, importer, exporter, CLI, and all eight scheduling algorithms.

---

## 8. New Follow-up Commit and Parent (Part E)

- **Commit Message**: `fix: preserve existing CSV exports during replacement`
- **Parent Commit**: `cf0aca7c52553a6276155ff413880fc74e368ed0` (Batch 12 original implementation commit)
- **Committed Files**:
  - `README.md`
  - `docs/ARCHITECTURE.md`
  - `include/cpu_scheduler/csv_exporter.hpp`
  - `src/csv_exporter.cpp`
  - `tests/test_csv_results_exporter.cpp`
  - `BATCH_12_FOLLOWUP_REPORT.md`

---

## 9. Final Working-Tree Status

- Branch: `main`
- All historical audit reports (`BATCH_5` through `BATCH_12`) remain untracked and preserved.
- No history was rewritten, rebased, squashed, or amended.
- No remote pushes were made.
