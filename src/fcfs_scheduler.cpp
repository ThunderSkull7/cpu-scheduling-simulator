#include "cpu_scheduler/fcfs_scheduler.hpp"
#include "cpu_scheduler/process_validator.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace cpu_scheduler {

namespace {

[[nodiscard]] bool safe_add(Time a, Time b, Time& out) noexcept {
    if (b > 0 && a > std::numeric_limits<Time>::max() - b) {
        return false;
    }
    if (b < 0 && a < std::numeric_limits<Time>::min() - b) {
        return false;
    }
    out = a + b;
    return true;
}

struct ProcessOrderEntry {
    std::size_t original_index;
    const Process* process;
};

} // namespace

ScheduleOutcome FcfsScheduler::schedule(std::span<const Process> processes) const {
    // 1. Validate the input workload
    const ValidationResult validation = ProcessValidator::validate(processes);
    if (!validation.is_valid()) {
        return ScheduleError{
            .kind = ScheduleErrorKind::ValidationError,
            .message = "Workload validation failed: workload contains invalid inputs.",
            .validation_result = validation
        };
    }

    // 2. Prepare stable execution ordering based on arrival time
    std::vector<ProcessOrderEntry> ordered;
    ordered.reserve(processes.size());
    for (std::size_t i = 0; i < processes.size(); ++i) {
        ordered.push_back(ProcessOrderEntry{i, &processes[i]});
    }

    // std::stable_sort preserves the original relative order for simultaneous arrivals
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const ProcessOrderEntry& a, const ProcessOrderEntry& b) {
                         return a.process->arrival_time < b.process->arrival_time;
                     });

    // 3. Initialize process states in original workload order
    std::vector<ProcessState> states;
    states.reserve(processes.size());
    for (const auto& proc : processes) {
        states.push_back(ProcessState::from_process(proc));
    }

    std::vector<ExecutionSegment> timeline;
    Time current_time = 0;
    Time total_busy_time = 0;
    Time total_idle_time = 0;

    // 4. Simulate FCFS execution
    for (const auto& entry : ordered) {
        const Process& proc = *(entry.process);
        ProcessState& state = states[entry.original_index];

        // Handle CPU idle interval when no process is ready
        if (proc.arrival_time > current_time) {
            const Time idle_duration = proc.arrival_time - current_time;
            timeline.push_back(ExecutionSegment::idle(current_time, proc.arrival_time));

            if (!safe_add(total_idle_time, idle_duration, total_idle_time)) {
                return ScheduleError{
                    .kind = ScheduleErrorKind::ArithmeticOverflow,
                    .message = "Arithmetic overflow occurred while accumulating CPU idle time.",
                    .validation_result = std::nullopt
                };
            }
            current_time = proc.arrival_time;
        }

        // Execute process
        const Time start_time = current_time;
        Time completion_time = 0;
        if (!safe_add(start_time, proc.burst_time, completion_time)) {
            return ScheduleError{
                .kind = ScheduleErrorKind::ArithmeticOverflow,
                .message = "Arithmetic overflow occurred while computing completion time for process '" +
                           proc.id + "'.",
                .validation_result = std::nullopt
            };
        }

        timeline.push_back(ExecutionSegment::process(proc.id, start_time, completion_time));

        if (!safe_add(total_busy_time, proc.burst_time, total_busy_time)) {
            return ScheduleError{
                .kind = ScheduleErrorKind::ArithmeticOverflow,
                .message = "Arithmetic overflow occurred while accumulating CPU busy time.",
                .validation_result = std::nullopt
            };
        }

        current_time = completion_time;

        // Record metrics for the process
        state.start_time = start_time;
        state.completion_time = completion_time;
        state.turnaround_time = completion_time - proc.arrival_time;
        state.waiting_time = state.turnaround_time - proc.burst_time;
        state.response_time = start_time - proc.arrival_time;
        state.remaining_time = 0;
        state.status = ProcessExecutionStatus::Completed;
    }

    // 5. Compute system performance metrics
    const Time total_simulation_time = current_time;
    double sum_waiting_time = 0.0;
    double sum_turnaround_time = 0.0;
    double sum_response_time = 0.0;

    for (const auto& s : states) {
        sum_waiting_time += static_cast<double>(s.waiting_time);
        sum_turnaround_time += static_cast<double>(s.turnaround_time);
        sum_response_time += static_cast<double>(s.response_time);
    }

    const double process_count = static_cast<double>(states.size());
    SchedulingMetrics metrics;
    metrics.total_execution_time = total_simulation_time;
    metrics.cpu_busy_time = total_busy_time;
    metrics.cpu_idle_time = total_idle_time;

    metrics.cpu_utilization = (total_simulation_time > 0)
        ? (static_cast<double>(total_busy_time) / static_cast<double>(total_simulation_time)) * 100.0
        : 0.0;

    metrics.average_waiting_time = (process_count > 0.0) ? (sum_waiting_time / process_count) : 0.0;
    metrics.average_turnaround_time = (process_count > 0.0) ? (sum_turnaround_time / process_count) : 0.0;
    metrics.average_response_time = (process_count > 0.0) ? (sum_response_time / process_count) : 0.0;

    return SchedulingResult(std::move(timeline), std::move(states), metrics);
}

} // namespace cpu_scheduler
