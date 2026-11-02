#include "cpu_scheduler/fcfs_scheduler.hpp"
#include "cpu_scheduler/gantt_chart.hpp"
#include "cpu_scheduler/process.hpp"
#include "cpu_scheduler/process_validator.hpp"

#include <iomanip>
#include <iostream>
#include <vector>

int main() {
    using namespace cpu_scheduler;

    std::cout << "=========================================================\n";
    std::cout << "      CPU Scheduling Simulator — Batch 2: FCFS Engine    \n";
    std::cout << "=========================================================\n\n";

    // 1. Define a workload with varied arrivals and an idle period
    const std::vector<Process> workload = {
        Process("P1", 0, 5, 2),
        Process("P2", 2, 3, 1),
        Process("P3", 4, 2, 3),
        Process("P4", 12, 4, 1)  // Idle period occurs between 10 and 12
    };

    std::cout << "Input Workload:\n";
    for (const auto& proc : workload) {
        std::cout << "  " << proc << "\n";
    }
    std::cout << "\nExecuting First Come, First Served (FCFS) Scheduling...\n\n";

    const auto outcome = FcfsScheduler::run(workload);

    if (!outcome.has_value()) {
        std::cerr << "Scheduling failed: " << outcome.error_message() << "\n";
        return 1;
    }

    const auto& result = outcome.value();

    // 2. Display Gantt Chart
    std::cout << "--- Gantt Chart Timeline ---\n\n";
    std::cout << GanttChart::render(result) << "\n";

    // 3. Display Detailed Process Metrics Table
    std::cout << "--- Process Execution Details ---\n";
    std::cout << std::left
              << std::setw(6)  << "PID"
              << std::setw(10) << "Arrival"
              << std::setw(8)  << "Burst"
              << std::setw(8)  << "Start"
              << std::setw(12) << "Completion"
              << std::setw(12) << "Turnaround"
              << std::setw(10) << "Waiting"
              << std::setw(10) << "Response"
              << "\n";
    std::cout << std::string(76, '-') << "\n";

    for (const auto& ps : result.processes()) {
        std::cout << std::left
                  << std::setw(6)  << ps.process_id
                  << std::setw(10) << ps.arrival_time
                  << std::setw(8)  << ps.burst_time
                  << std::setw(8)  << ps.start_time.value_or(-1)
                  << std::setw(12) << ps.completion_time
                  << std::setw(12) << ps.turnaround_time
                  << std::setw(10) << ps.waiting_time
                  << std::setw(10) << ps.response_time
                  << "\n";
    }

    // 4. Summary Metrics
    std::cout << "\n--- Aggregate Performance Metrics ---\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total Simulation Time: " << result.total_execution_time() << "\n";
    std::cout << "CPU Busy Time:         " << result.cpu_busy_time() << "\n";
    std::cout << "CPU Idle Time:         " << result.cpu_idle_time() << "\n";
    std::cout << "CPU Utilization:       " << result.cpu_utilization() << "%\n";
    std::cout << "Average Turnaround:    " << result.average_turnaround_time() << "\n";
    std::cout << "Average Waiting Time:  " << result.average_waiting_time() << "\n";
    std::cout << "Average Response Time: " << result.average_response_time() << "\n";
    std::cout << "=========================================================\n";

    return 0;
}
