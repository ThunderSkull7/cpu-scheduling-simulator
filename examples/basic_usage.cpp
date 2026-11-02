#include "cpu_scheduler/process.hpp"
#include "cpu_scheduler/process_validator.hpp"

#include <iostream>
#include <vector>

int main() {
    using namespace cpu_scheduler;

    std::cout << "=== CPU Scheduling Simulator - Batch 1 Foundation ===\n\n";

    // 1. Define a valid sample workload
    const std::vector<Process> valid_workload = {
        Process("P1", 0, 8, 3),
        Process("P2", 1, 4, 1),
        Process("P3", 2, 9, 4),
        Process("P4", 3, 5, 2)
    };

    std::cout << "Validating workload with " << valid_workload.size() << " processes...\n";
    const auto result_valid = ProcessValidator::validate(valid_workload);

    if (result_valid.is_valid()) {
        std::cout << "[PASS] " << result_valid.summary() << "\n\nProcesses:\n";
        for (const auto& proc : valid_workload) {
            std::cout << "  " << proc << "\n";
        }
    }

    std::cout << "\n----------------------------------------\n\n";

    // 2. Define an invalid workload demonstrating validation error detection
    const std::vector<Process> invalid_workload = {
        Process("P1", -2, 5, 1),       // Negative arrival time
        Process(" p1 ", 0, 0, 2),       // Duplicate ID (normalized) & Zero burst time
        Process("  ", 4, 3, 3)          // Empty / whitespace ID
    };

    std::cout << "Validating invalid workload with deliberate errors...\n";
    const auto result_invalid = ProcessValidator::validate(invalid_workload);

    if (!result_invalid.is_valid()) {
        std::cout << "[EXPECTED ERRORS]\n" << result_invalid.summary();
    }

    return 0;
}
