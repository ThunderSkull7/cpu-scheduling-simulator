# CPU Scheduling Simulator

A high-performance, modular C++20 simulator for standard Operating System CPU scheduling algorithms. The simulator provides deterministic execution tracking, comprehensive validation, Gantt chart visualization, performance metric computation, and comparative algorithm analysis.

---

## Current Status: Batch 1 — Project Foundation

- [x] **Core Process Model**: Immutable workload data structure (`Process`) with 64-bit time representations.
- [x] **Input Validation Engine**: Comprehensive workload verification (`ProcessValidator`) with multi-error reporting.
- [x] **Modern Build System**: CMake presets with vcpkg manifest mode.
- [x] **Unit Testing Suite**: GoogleTest integration covering all core model and validation edge cases.
- [x] **Documentation**: Architectural boundaries and separation of concerns documented.
- [ ] **Scheduling Algorithms** *(Planned - Batch 2)*
- [ ] **Gantt Chart & Metrics** *(Planned - Batch 3)*
- [ ] **Interactive CLI & Comparison View** *(Planned - Batch 4)*

---

## Planned Scheduling Algorithms

The completed simulator will implement the following scheduling strategies:

1. **First Come First Served (FCFS)**: Non-preemptive, FIFO arrival-based execution.
2. **Shortest Job First (SJF)**: Non-preemptive, execution ordered by shortest total burst time.
3. **Shortest Remaining Time First (SRTF)**: Preemptive SJF, switching on shorter incoming remaining execution time.
4. **Priority Scheduling**: Both preemptive and non-preemptive variants evaluated against process priority integers.
5. **Round Robin (RR)**: Preemptive scheduling with configurable time-slice quanta.

---

## Technology Stack

- **Standard**: C++20 (`/std:c++20`, `-std=c++20`)
- **Compilers**: MSVC (Visual Studio 2022 v17.x) / Clang / GCC
- **Build System**: CMake 3.20+ with CMakePresets (Version 3)
- **Package Manager**: vcpkg (Manifest Mode)
- **Testing**: GoogleTest (via vcpkg)
- **Runtime Dependencies**: ISO C++ Standard Library only

---

## Prerequisites

1. **Visual Studio 2022** (v17.4+ recommended) with the "Desktop development with C++" workload installed.
2. **CMake 3.20+** (bundled with VS 2022 or installed on `PATH`).
3. **vcpkg** installed and configured via the `VCPKG_ROOT` environment variable.

---

## Build Instructions

The project uses `CMakePresets.json` for configuration and compilation.

### 1. Environment Setup

Ensure `VCPKG_ROOT` points to your vcpkg installation:

```powershell
# PowerShell
$env:VCPKG_ROOT = "C:\vcpkg" # or your local vcpkg directory
```

### 2. Configure Presets

#### Debug Configuration
```powershell
cmake --preset windows-msvc-debug
```

#### Release Configuration
```powershell
cmake --preset windows-msvc-release
```

### 3. Build Presets

#### Build Debug
```powershell
cmake --build --preset debug
```

#### Build Release
```powershell
cmake --build --preset release
```

---

## Running Unit Tests

Unit tests are managed via CTest and executed using CMake test presets:

#### Run Debug Tests
```powershell
ctest --preset debug-test
```

#### Run Release Tests
```powershell
ctest --preset release-test
```

#### Run GoogleTest Executable Directly
```powershell
# Debug binary
.\build\debug\tests\Debug\cpu_scheduler_tests.exe

# Release binary
.\build\release\tests\Release\cpu_scheduler_tests.exe
```

---

## Running Example Application

An executable example demonstrating process construction and workload validation is built automatically:

```powershell
# Debug
.\build\debug\examples\Debug\basic_usage.exe

# Release
.\build\release\examples\Release\basic_usage.exe
```

---

## Architecture & Code Structure

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for architectural details and component boundaries.

```text
CPU Scheduling Simulator/
│
├── CMakeLists.txt              # Top-level CMake project configuration
├── CMakePresets.json           # Multi-configuration presets (MSVC x64 Debug/Release)
├── vcpkg.json                  # vcpkg manifest defining dependencies (GoogleTest)
├── .gitignore                  # Git ignore rules for build artifacts and IDE files
├── README.md                   # Project overview, setup, and build instructions
│
├── include/
│   └── cpu_scheduler/
│       ├── process.hpp          # Immutable process workload model
│       └── process_validator.hpp# Workload validation interface
│
├── src/
│   ├── process.cpp             # Process stream output implementation
│   └── process_validator.cpp   # Workload validation implementation
│
├── tests/
│   ├── CMakeLists.txt          # GoogleTest suite target
│   ├── test_process.cpp        # Tests for process data model
│   └── test_process_validator.cpp # Tests for validation logic and edge cases
│
├── docs/
│   └── ARCHITECTURE.md         # Component boundaries and lifecycle documentation
│
└── examples/
    ├── CMakeLists.txt          # Example runner target
    └── basic_usage.cpp         # Demonstrates API usage and error inspection
```
