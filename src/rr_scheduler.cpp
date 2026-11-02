#include "cpu_scheduler/rr_scheduler.hpp"
#include "cpu_scheduler/process_validator.hpp"

#include <algorithm>
#include <deque>
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

struct ProcessEntry {
    std::size_t original_index;
    const Process* process;
};

struct ReadyEntry {
    std::size_t original_index;
    const Process* process;
    Time remaining_time;
};

void append_normalized_segment(std::vector<ExecutionSegment>& timeline,
                               SegmentType type,
                               std::optional<std::string> pid,
                               Time start,
                               Time end) {
    if (start >= end) {
        return;
    }
    if (!timeline.empty() &&
        timeline.back().type == type &&
        timeline.back().process_id == pid &&
        timeline.back().end_time == start) {
        // Normalize: Merge consecutive adjacent allocations for the same process or idle
        timeline.back().end_time = end;
    } else {
        timeline.push_back(ExecutionSegment(type, std::move(pid), start, end));
    }
}

} // namespace

ScheduleOutcome RrScheduler::schedule(std::span<const Process> processes) const {
    return schedule(processes, time_quantum_);
}

ScheduleOutcome RrScheduler::schedule(std::span<const Process> processes, Time time_quantum) const {
    // 1. Validate workload integrity
    const ValidationResult validation = ProcessValidator::validate(processes);
    if (!validation.is_valid()) {
        return ScheduleError{
            .kind = ScheduleErrorKind::ValidationError,
            .message = "Workload validation failed: workload contains invalid inputs.",
            .validation_result = validation
        };
    }

    // 2. Validate time quantum
    if (time_quantum <= 0) {
        return ScheduleError{
            .kind = ScheduleErrorKind::ValidationError,
            .message = "Time quantum must be strictly positive (greater than 0).",
            .validation_result = std::nullopt
        };
    }

    // 3. Prepare unarrived process collection ordered by arrival time
    std::vector<ProcessEntry> unarrived;
    unarrived.reserve(processes.size());
    for (std::size_t i = 0; i < processes.size(); ++i) {
        unarrived.push_back(ProcessEntry{i, &processes[i]});
    }

    std::stable_sort(unarrived.begin(), unarrived.end(),
                     [](const ProcessEntry& a, const ProcessEntry& b) {
                         if (a.process->arrival_time != b.process->arrival_time) {
                             return a.process->arrival_time < b.process->arrival_time;
                         }
                         return a.original_index < b.original_index;
                     });

    // 4. Initialize mutable process execution states in original input order
    std::vector<ProcessState> states;
    states.reserve(processes.size());
    for (const auto& proc : processes) {
        states.push_back(ProcessState::from_process(proc));
    }

    std::vector<ExecutionSegment> timeline;
    std::deque<ReadyEntry> ready_queue;

    std::size_t unarrived_idx = 0;
    Time current_time = 0;
    Time total_busy_time = 0;
    Time total_idle_time = 0;

    // 5. Event-driven Round Robin simulation loop
    while (!ready_queue.empty() || unarrived_idx < unarrived.size()) {
        // Step A: If ready_queue is empty, advance to next process arrival
        if (ready_queue.empty()) {
            const auto& next_entry = unarrived[unarrived_idx];
            if (next_entry.process->arrival_time > current_time) {
                const Time idle_duration = next_entry.process->arrival_time - current_time;
                append_normalized_segment(timeline, SegmentType::Idle, std::nullopt, current_time, next_entry.process->arrival_time);

                if (!safe_add(total_idle_time, idle_duration, total_idle_time)) {
                    return ScheduleError{
                        .kind = ScheduleErrorKind::ArithmeticOverflow,
                        .message = "Arithmetic overflow occurred while accumulating CPU idle time.",
                        .validation_result = std::nullopt
                    };
                }
                current_time = next_entry.process->arrival_time;
            }

            // Admit all processes arriving up to current_time
            while (unarrived_idx < unarrived.size() &&
                   unarrived[unarrived_idx].process->arrival_time <= current_time) {
                const auto& entry = unarrived[unarrived_idx++];
                ready_queue.push_back(ReadyEntry{
                    .original_index = entry.original_index,
                    .process = entry.process,
                    .remaining_time = entry.process->burst_time
                });
            }
        }

        if (ready_queue.empty()) {
            break;
        }

        // Step B: Pop front process from FIFO ready queue
        ReadyEntry current_proc = ready_queue.front();
        ready_queue.pop_front();

        ProcessState& state = states[current_proc.original_index];
        if (!state.start_time.has_value()) {
            state.start_time = current_time;
        }
        state.status = ProcessExecutionStatus::Running;

        // Step C: Execute for min(time_quantum, remaining_time)
        const Time slice_duration = std::min(time_quantum, current_proc.remaining_time);
        const Time slice_start = current_time;
        Time slice_end = 0;
        if (!safe_add(slice_start, slice_duration, slice_end)) {
            return ScheduleError{
                .kind = ScheduleErrorKind::ArithmeticOverflow,
                .message = "Arithmetic overflow occurred while computing slice end time for process '" +
                           current_proc.process->id + "'.",
                .validation_result = std::nullopt
            };
        }

        if (!safe_add(total_busy_time, slice_duration, total_busy_time)) {
            return ScheduleError{
                .kind = ScheduleErrorKind::ArithmeticOverflow,
                .message = "Arithmetic overflow occurred while accumulating CPU busy time.",
                .validation_result = std::nullopt
            };
        }

        current_time = slice_end;
        current_proc.remaining_time -= slice_duration;

        // Append execution segment with normalization for adjacent slices
        append_normalized_segment(timeline, SegmentType::Process, current_proc.process->id, slice_start, slice_end);

        // Step D: Admit newly arrived processes up to current_time BEFORE requeuing expired process
        while (unarrived_idx < unarrived.size() &&
               unarrived[unarrived_idx].process->arrival_time <= current_time) {
            const auto& entry = unarrived[unarrived_idx++];
            ready_queue.push_back(ReadyEntry{
                .original_index = entry.original_index,
                .process = entry.process,
                .remaining_time = entry.process->burst_time
            });
        }

        // Step E: Handle process completion or requeue
        if (current_proc.remaining_time == 0) {
            state.completion_time = current_time;
            state.turnaround_time = current_time - current_proc.process->arrival_time;
            state.waiting_time = state.turnaround_time - current_proc.process->burst_time;
            state.response_time = state.start_time.value() - current_proc.process->arrival_time;
            state.remaining_time = 0;
            state.status = ProcessExecutionStatus::Completed;
        } else {
            ready_queue.push_back(current_proc);
        }
    }

    // 6. Calculate system-wide summary metrics
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
