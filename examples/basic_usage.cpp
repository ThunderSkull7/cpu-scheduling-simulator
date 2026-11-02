#include "cpu_scheduler/fcfs_scheduler.hpp"
#include "cpu_scheduler/gantt_chart.hpp"
#include "cpu_scheduler/priority_scheduler.hpp"
#include "cpu_scheduler/process.hpp"
#include "cpu_scheduler/rr_scheduler.hpp"
#include "cpu_scheduler/sjf_scheduler.hpp"
#include "cpu_scheduler/srtf_scheduler.hpp"

#include <iomanip>
#include <iostream>
#include <vector>

namespace {

void display_schedule_result(std::string_view algorithm_name,
                             const cpu_scheduler::SchedulingResult& result) {
    std::cout << "---------------------------------------------------------\n";
    std::cout << " Algorithm: " << algorithm_name << "\n";
    std::cout << "---------------------------------------------------------\n";

    // 1. Process Execution Order
    std::cout << "Execution Order: ";
    bool first = true;
    for (const auto& seg : result.timeline()) {
        if (seg.is_process()) {
            if (!first) std::cout << " -> ";
            std::cout << seg.process_id.value_or("?");
            first = false;
        }
    }
    std::cout << "\n\n";

    // 2. Gantt Chart
    std::cout << "Gantt Chart:\n";
    std::cout << cpu_scheduler::GanttChart::render(result) << "\n";

    // 3. Process Metrics Table
    std::cout << std::left
              << std::setw(6)  << "PID"
              << std::setw(10) << "Arrival"
              << std::setw(8)  << "Burst"
              << std::setw(10) << "Priority"
              << std::setw(8)  << "Start"
              << std::setw(12) << "Completion"
              << std::setw(12) << "Turnaround"
              << std::setw(10) << "Waiting"
              << std::setw(10) << "Response"
              << "\n";
    std::cout << std::string(86, '-') << "\n";

    for (const auto& ps : result.processes()) {
        std::cout << std::left
                  << std::setw(6)  << ps.process_id
                  << std::setw(10) << ps.arrival_time
                  << std::setw(8)  << ps.burst_time
                  << std::setw(10) << ps.priority
                  << std::setw(8)  << ps.start_time.value_or(-1)
                  << std::setw(12) << ps.completion_time
                  << std::setw(12) << ps.turnaround_time
                  << std::setw(10) << ps.waiting_time
                  << std::setw(10) << ps.response_time
                  << "\n";
    }

    // 4. Summary Metrics
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\nAverage Waiting Time:  " << result.average_waiting_time() << "\n";
    std::cout << "Average Turnaround:    " << result.average_turnaround_time() << "\n";
    std::cout << "CPU Utilization:       " << result.cpu_utilization() << "%\n\n";
}

} // namespace

int main() {
    using namespace cpu_scheduler;

    std::cout << "=========================================================\n";
    std::cout << "  CPU Scheduling Simulator — Comprehensive Comparison    \n";
    std::cout << "=========================================================\n\n";

    // Standard comparison workload with priorities assigned (smaller value = higher priority)
    const std::vector<Process> workload = {
        Process("P1", 0, 7, 3),
        Process("P2", 2, 4, 1),
        Process("P3", 4, 1, 2),
        Process("P4", 5, 4, 1)
    };

    std::cout << "Input Workload:\n";
    for (const auto& proc : workload) {
        std::cout << "  " << proc << "\n";
    }
    std::cout << "\n";

    // 1. Run FCFS
    const auto fcfs_outcome = FcfsScheduler::run(workload);
    if (fcfs_outcome.has_value()) {
        display_schedule_result("First Come, First Served (FCFS)", fcfs_outcome.value());
    } else {
        std::cerr << "FCFS failed: " << fcfs_outcome.error_message() << "\n";
    }

    // 2. Run SJF
    const auto sjf_outcome = SjfScheduler::run(workload);
    if (sjf_outcome.has_value()) {
        display_schedule_result("Shortest Job First (SJF, Non-Preemptive)", sjf_outcome.value());
    } else {
        std::cerr << "SJF failed: " << sjf_outcome.error_message() << "\n";
    }

    // 3. Run SRTF (Preemptive SJF)
    const auto srtf_outcome = SrtfScheduler::run(workload);
    if (srtf_outcome.has_value()) {
        display_schedule_result("Shortest Remaining Time First (SRTF, Preemptive)", srtf_outcome.value());
    } else {
        std::cerr << "SRTF failed: " << srtf_outcome.error_message() << "\n";
    }

    // 4. Run Round Robin (RR, Quantum = 2)
    constexpr Time rr_quantum = 2;
    const auto rr_outcome = RrScheduler::run(workload, rr_quantum);
    if (rr_outcome.has_value()) {
        display_schedule_result("Round Robin (RR, Quantum = 2)", rr_outcome.value());
    } else {
        std::cerr << "Round Robin failed: " << rr_outcome.error_message() << "\n";
    }

    // 5. Run Non-Preemptive Priority Scheduling
    const auto np_prio_outcome = NonPreemptivePriorityScheduler::run(workload);
    if (np_prio_outcome.has_value()) {
        display_schedule_result("Non-Preemptive Priority Scheduling", np_prio_outcome.value());
    } else {
        std::cerr << "Non-Preemptive Priority failed: " << np_prio_outcome.error_message() << "\n";
    }

    // 6. Run Preemptive Priority Scheduling
    const auto p_prio_outcome = PreemptivePriorityScheduler::run(workload);
    if (p_prio_outcome.has_value()) {
        display_schedule_result("Preemptive Priority Scheduling", p_prio_outcome.value());
    } else {
        std::cerr << "Preemptive Priority failed: " << p_prio_outcome.error_message() << "\n";
    }

    std::cout << "=========================================================\n";
    return 0;
}
