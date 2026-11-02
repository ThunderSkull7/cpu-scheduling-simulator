#include "cpu_scheduler/priority_scheduler.hpp"
#include "cpu_scheduler/process_validator.hpp"

#include <algorithm>
#include <limits>
#include <numeric>
#include <optional>
#include <queue>
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

/// Comparator for non-preemptive ready priority queue (min-heap ordering).
/// In std::priority_queue, operator() returns true if `a` has strictly LOWER scheduling priority than `b`.
struct NonPreemptivePriorityComparator {
    bool operator()(const ProcessEntry& a, const ProcessEntry& b) const noexcept {
        // 1. Smaller priority value = strictly higher scheduling priority
        if (a.process->priority != b.process->priority) {
            return a.process->priority > b.process->priority;
        }
        // 2. Earlier arrival time breaks ties
        if (a.process->arrival_time != b.process->arrival_time) {
            return a.process->arrival_time > b.process->arrival_time;
        }
        // 3. Preserve original input workload sequence index
        return a.original_index > b.original_index;
    }
};

struct ReadyCandidate {
    std::size_t original_index;
    const Process* process;
    Time remaining_time;
};

/// Comparator for preemptive ready priority queue (min-heap ordering).
/// In std::priority_queue, operator() returns true if `a` has strictly LOWER scheduling priority than `b`.
struct PreemptivePriorityComparator {
    bool operator()(const ReadyCandidate& a, const ReadyCandidate& b) const noexcept {
        // 1. Smaller priority value = strictly higher scheduling priority
        if (a.process->priority != b.process->priority) {
            return a.process->priority > b.process->priority;
        }
        // 2. Earlier arrival time breaks ties
        if (a.process->arrival_time != b.process->arrival_time) {
            return a.process->arrival_time > b.process->arrival_time;
        }
        // 3. Preserve original input workload sequence index
        return a.original_index > b.original_index;
    }
};

struct ActiveExecution {
    std::size_t original_index;
    const Process* process;
    Time slice_start_time;
    Time remaining_time;
};

} // namespace

// ============================================================================
// NonPreemptivePriorityScheduler Implementation
// ============================================================================

ScheduleOutcome NonPreemptivePriorityScheduler::schedule(std::span<const Process> processes) const {
    // 1. Validate the input workload
    const ValidationResult validation = ProcessValidator::validate(processes);
    if (!validation.is_valid()) {
        return ScheduleError{
            .kind = ScheduleErrorKind::ValidationError,
            .message = "Workload validation failed: workload contains invalid inputs.",
            .validation_result = validation
        };
    }

    // 2. Prepare unarrived process collection ordered by arrival time
    std::vector<ProcessEntry> unarrived;
    unarrived.reserve(processes.size());
    for (std::size_t i = 0; i < processes.size(); ++i) {
        unarrived.push_back(ProcessEntry{i, &processes[i]});
    }

    // Sort unarrived processes primarily by arrival time, maintaining input stability
    std::stable_sort(unarrived.begin(), unarrived.end(),
                     [](const ProcessEntry& a, const ProcessEntry& b) {
                         if (a.process->arrival_time != b.process->arrival_time) {
                             return a.process->arrival_time < b.process->arrival_time;
                         }
                         return a.original_index < b.original_index;
                     });

    // 3. Initialize process states in original workload sequence
    std::vector<ProcessState> states;
    states.reserve(processes.size());
    for (const auto& proc : processes) {
        states.push_back(ProcessState::from_process(proc));
    }

    std::vector<ExecutionSegment> timeline;
    std::priority_queue<ProcessEntry, std::vector<ProcessEntry>, NonPreemptivePriorityComparator> ready_queue;

    std::size_t unarrived_idx = 0;
    Time current_time = 0;
    Time total_busy_time = 0;
    Time total_idle_time = 0;

    // 4. Dynamic non-preemptive priority simulation loop
    while (!ready_queue.empty() || unarrived_idx < unarrived.size()) {
        // If ready queue is empty, fast-forward time to next process arrival
        if (ready_queue.empty()) {
            const auto& next_proc = *(unarrived[unarrived_idx].process);
            if (next_proc.arrival_time > current_time) {
                const Time idle_duration = next_proc.arrival_time - current_time;
                timeline.push_back(ExecutionSegment::idle(current_time, next_proc.arrival_time));

                if (!safe_add(total_idle_time, idle_duration, total_idle_time)) {
                    return ScheduleError{
                        .kind = ScheduleErrorKind::ArithmeticOverflow,
                        .message = "Arithmetic overflow occurred while accumulating CPU idle time.",
                        .validation_result = std::nullopt
                    };
                }
                current_time = next_proc.arrival_time;
            }

            // Enqueue all processes that have arrived up to current_time
            while (unarrived_idx < unarrived.size() &&
                   unarrived[unarrived_idx].process->arrival_time <= current_time) {
                ready_queue.push(unarrived[unarrived_idx++]);
            }
        }

        // Select the process with highest priority (smallest numerical value, with tie-breaking)
        const ProcessEntry selected = ready_queue.top();
        ready_queue.pop();

        const Process& proc = *(selected.process);
        ProcessState& state = states[selected.original_index];

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

        // Record process metrics
        state.start_time = start_time;
        state.completion_time = completion_time;
        state.turnaround_time = completion_time - proc.arrival_time;
        state.waiting_time = state.turnaround_time - proc.burst_time;
        state.response_time = start_time - proc.arrival_time;
        state.remaining_time = 0;
        state.status = ProcessExecutionStatus::Completed;

        // Enqueue all processes that arrived during this process's execution
        while (unarrived_idx < unarrived.size() &&
               unarrived[unarrived_idx].process->arrival_time <= current_time) {
            ready_queue.push(unarrived[unarrived_idx++]);
        }
    }

    // 5. Compute system-wide scheduling metrics
    const Time total_execution_time = current_time;
    const double cpu_utilization = (total_execution_time > 0)
        ? (static_cast<double>(total_busy_time) / static_cast<double>(total_execution_time)) * 100.0
        : 0.0;

    double sum_waiting_time = 0.0;
    double sum_turnaround_time = 0.0;
    double sum_response_time = 0.0;

    for (const auto& s : states) {
        sum_waiting_time += static_cast<double>(s.waiting_time);
        sum_turnaround_time += static_cast<double>(s.turnaround_time);
        sum_response_time += static_cast<double>(s.response_time);
    }

    const double n = static_cast<double>(states.size());
    const SchedulingMetrics metrics{
        .total_execution_time = total_execution_time,
        .cpu_busy_time = total_busy_time,
        .cpu_idle_time = total_idle_time,
        .cpu_utilization = cpu_utilization,
        .average_waiting_time = sum_waiting_time / n,
        .average_turnaround_time = sum_turnaround_time / n,
        .average_response_time = sum_response_time / n
    };

    return SchedulingResult(std::move(timeline), std::move(states), metrics);
}

// ============================================================================
// PreemptivePriorityScheduler Implementation
// ============================================================================

ScheduleOutcome PreemptivePriorityScheduler::schedule(std::span<const Process> processes) const {
    // 1. Validate workload integrity
    const ValidationResult validation = ProcessValidator::validate(processes);
    if (!validation.is_valid()) {
        return ScheduleError{
            .kind = ScheduleErrorKind::ValidationError,
            .message = "Workload validation failed: workload contains invalid inputs.",
            .validation_result = validation
        };
    }

    // 2. Prepare unarrived process collection ordered by arrival time
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

    // 3. Initialize mutable process execution states in original input order
    std::vector<ProcessState> states;
    states.reserve(processes.size());
    for (const auto& proc : processes) {
        states.push_back(ProcessState::from_process(proc));
    }

    std::vector<ExecutionSegment> timeline;
    std::priority_queue<ReadyCandidate, std::vector<ReadyCandidate>, PreemptivePriorityComparator> ready_queue;
    std::optional<ActiveExecution> active = std::nullopt;

    std::size_t unarrived_idx = 0;
    Time current_time = 0;
    Time total_busy_time = 0;
    Time total_idle_time = 0;

    // 4. Event-driven preemptive priority simulation loop
    while (active.has_value() || !ready_queue.empty() || unarrived_idx < unarrived.size()) {
        // Step A: Admit all processes that have arrived at or before current_time
        while (unarrived_idx < unarrived.size() &&
               unarrived[unarrived_idx].process->arrival_time <= current_time) {
            const auto& entry = unarrived[unarrived_idx++];
            ready_queue.push(ReadyCandidate{
                .original_index = entry.original_index,
                .process = entry.process,
                .remaining_time = entry.process->burst_time
            });
        }

        // Step B: If CPU is currently idle, either jump to next arrival or dispatch from ready queue
        if (!active.has_value()) {
            if (ready_queue.empty()) {
                if (unarrived_idx < unarrived.size()) {
                    const Time next_arrival = unarrived[unarrived_idx].process->arrival_time;
                    if (next_arrival > current_time) {
                        const Time idle_duration = next_arrival - current_time;
                        timeline.push_back(ExecutionSegment::idle(current_time, next_arrival));

                        if (!safe_add(total_idle_time, idle_duration, total_idle_time)) {
                            return ScheduleError{
                                .kind = ScheduleErrorKind::ArithmeticOverflow,
                                .message = "Arithmetic overflow occurred while accumulating CPU idle time.",
                                .validation_result = std::nullopt
                            };
                        }
                        current_time = next_arrival;
                    }

                    // Admit newly arrived processes at this advanced timestamp
                    while (unarrived_idx < unarrived.size() &&
                           unarrived[unarrived_idx].process->arrival_time <= current_time) {
                        const auto& entry = unarrived[unarrived_idx++];
                        ready_queue.push(ReadyCandidate{
                            .original_index = entry.original_index,
                            .process = entry.process,
                            .remaining_time = entry.process->burst_time
                        });
                    }
                } else {
                    // All processes have arrived, completed, and ready queue is empty
                    break;
                }
            }

            // Dispatch highest-priority candidate from ready queue
            if (!ready_queue.empty()) {
                const ReadyCandidate selected = ready_queue.top();
                ready_queue.pop();

                ProcessState& state = states[selected.original_index];
                if (!state.start_time.has_value()) {
                    state.start_time = current_time;
                }
                state.status = ProcessExecutionStatus::Running;

                active = ActiveExecution{
                    .original_index = selected.original_index,
                    .process = selected.process,
                    .slice_start_time = current_time,
                    .remaining_time = selected.remaining_time
                };
            }
        }

        // Step C: Preemption check for active process
        // A ready process preempts the running process IF AND ONLY IF it has a STRICTLY higher priority
        // (i.e. smaller numerical priority value). Equal priority arrivals DO NOT trigger preemption.
        if (active.has_value() && !ready_queue.empty()) {
            const ReadyCandidate& best_ready = ready_queue.top();
            if (best_ready.process->priority < active->process->priority) {
                // Preempt running process: record executed slice if duration > 0
                if (current_time > active->slice_start_time) {
                    timeline.push_back(ExecutionSegment::process(active->process->id,
                                                                 active->slice_start_time,
                                                                 current_time));
                }

                // Return preempted process to ready pool with remaining burst
                ready_queue.push(ReadyCandidate{
                    .original_index = active->original_index,
                    .process = active->process,
                    .remaining_time = active->remaining_time
                });

                // Context switch to the strictly higher-priority ready process
                const ReadyCandidate next_proc = ready_queue.top();
                ready_queue.pop();

                ProcessState& next_state = states[next_proc.original_index];
                if (!next_state.start_time.has_value()) {
                    next_state.start_time = current_time;
                }
                next_state.status = ProcessExecutionStatus::Running;

                active = ActiveExecution{
                    .original_index = next_proc.original_index,
                    .process = next_proc.process,
                    .slice_start_time = current_time,
                    .remaining_time = next_proc.remaining_time
                };
            }
        }

        // Step D: Advance time to next event (process completion or next arrival)
        if (active.has_value()) {
            Time projected_completion = 0;
            if (!safe_add(current_time, active->remaining_time, projected_completion)) {
                return ScheduleError{
                    .kind = ScheduleErrorKind::ArithmeticOverflow,
                    .message = "Arithmetic overflow occurred while computing completion time for process '" +
                               active->process->id + "'.",
                    .validation_result = std::nullopt
                };
            }

            if (unarrived_idx < unarrived.size()) {
                const Time next_arrival = unarrived[unarrived_idx].process->arrival_time;
                if (next_arrival < projected_completion) {
                    // Next event is an arrival occurring before active process completion
                    const Time duration = next_arrival - current_time;
                    if (!safe_add(total_busy_time, duration, total_busy_time)) {
                        return ScheduleError{
                            .kind = ScheduleErrorKind::ArithmeticOverflow,
                            .message = "Arithmetic overflow occurred while accumulating CPU busy time.",
                            .validation_result = std::nullopt
                        };
                    }

                    current_time = next_arrival;
                    active->remaining_time -= duration;
                    // Active process continues running; slice_start_time is preserved.
                    // Loop back to admit arrivals and evaluate preemption.
                    continue;
                }
            }

            // No arrival occurs before completion; active process runs uninterrupted to completion
            const Time duration = active->remaining_time;
            if (!safe_add(total_busy_time, duration, total_busy_time)) {
                return ScheduleError{
                    .kind = ScheduleErrorKind::ArithmeticOverflow,
                    .message = "Arithmetic overflow occurred while accumulating CPU busy time.",
                    .validation_result = std::nullopt
                };
            }

            timeline.push_back(ExecutionSegment::process(active->process->id,
                                                         active->slice_start_time,
                                                         projected_completion));
            current_time = projected_completion;

            // Finalize completed process metrics
            ProcessState& state = states[active->original_index];
            state.completion_time = current_time;
            state.turnaround_time = current_time - active->process->arrival_time;
            state.waiting_time = state.turnaround_time - active->process->burst_time;
            state.response_time = state.start_time.value() - active->process->arrival_time;
            state.remaining_time = 0;
            state.status = ProcessExecutionStatus::Completed;

            active = std::nullopt;
        }
    }

    // 5. Compute system-wide scheduling metrics
    const Time total_execution_time = current_time;
    const double cpu_utilization = (total_execution_time > 0)
        ? (static_cast<double>(total_busy_time) / static_cast<double>(total_execution_time)) * 100.0
        : 0.0;

    double sum_waiting_time = 0.0;
    double sum_turnaround_time = 0.0;
    double sum_response_time = 0.0;

    for (const auto& s : states) {
        sum_waiting_time += static_cast<double>(s.waiting_time);
        sum_turnaround_time += static_cast<double>(s.turnaround_time);
        sum_response_time += static_cast<double>(s.response_time);
    }

    const double n = static_cast<double>(states.size());
    const SchedulingMetrics metrics{
        .total_execution_time = total_execution_time,
        .cpu_busy_time = total_busy_time,
        .cpu_idle_time = total_idle_time,
        .cpu_utilization = cpu_utilization,
        .average_waiting_time = sum_waiting_time / n,
        .average_turnaround_time = sum_turnaround_time / n,
        .average_response_time = sum_response_time / n
    };

    return SchedulingResult(std::move(timeline), std::move(states), metrics);
}

} // namespace cpu_scheduler
