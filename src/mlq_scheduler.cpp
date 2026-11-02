#include "cpu_scheduler/mlq_scheduler.hpp"
#include "cpu_scheduler/process_validator.hpp"

#include <algorithm>
#include <cctype>
#include <deque>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
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

[[nodiscard]] std::string normalize_id(std::string_view str) {
    auto trimmed = ProcessValidator::trim_whitespace(str);
    std::string res;
    res.reserve(trimmed.size());
    for (char c : trimmed) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

void append_normalized_segment(std::vector<ExecutionSegment>& timeline,
                               const std::string& process_id,
                               Time start,
                               Time end) {
    if (start >= end) {
        return;
    }
    if (!timeline.empty() && !timeline.back().is_idle() &&
        timeline.back().process_id == process_id &&
        timeline.back().end_time == start) {
        timeline.back().end_time = end;
    } else {
        timeline.push_back(ExecutionSegment::process(process_id, start, end));
    }
}

struct ProcessEntry {
    std::size_t original_index;
    const Process* process;
    std::size_t queue_index;
    Time remaining_burst;
};

struct QueueInternalState {
    MlqQueueConfig config;
    std::size_t config_index; // original position in workload.queues for tie-breaking
    Time effective_iq_quantum;

    // Ready processes within this queue
    std::deque<std::size_t> ready_deque; // for FCFS & RR
    std::vector<std::size_t> ready_pool;  // for SJF, SRTF, Priority

    std::optional<std::size_t> active_proc_idx{std::nullopt};
    Time active_internal_q_remaining{0};

    [[nodiscard]] bool has_ready_processes() const noexcept {
        return active_proc_idx.has_value() || !ready_deque.empty() || !ready_pool.empty();
    }
};

} // namespace

ScheduleOutcome MlqScheduler::schedule(const MlqWorkload& workload) const {
    // ------------------------------------------------------------------------
    // 1. Validation
    // ------------------------------------------------------------------------
    if (workload.queues.empty()) {
        return ScheduleError{
            .kind = ScheduleErrorKind::ValidationError,
            .message = "MLQ validation failed: at least one queue must be configured.",
            .validation_result = std::nullopt
        };
    }

    if (workload.processes.empty()) {
        std::vector<Process> empty_procs;
        ValidationResult res = ProcessValidator::validate(empty_procs);
        return ScheduleError{
            .kind = ScheduleErrorKind::ValidationError,
            .message = "MLQ validation failed: workload contains no processes.",
            .validation_result = res
        };
    }

    // Validate Queue Configurations
    std::unordered_set<std::string> seen_queue_ids;
    for (std::size_t i = 0; i < workload.queues.size(); ++i) {
        const auto& q = workload.queues[i];
        if (ProcessValidator::is_blank(q.id)) {
            return ScheduleError{
                .kind = ScheduleErrorKind::ValidationError,
                .message = "MLQ validation failed: queue ID at index " + std::to_string(i) + " is empty or whitespace.",
                .validation_result = std::nullopt
            };
        }

        const std::string norm_qid = normalize_id(q.id);
        if (!seen_queue_ids.insert(norm_qid).second) {
            return ScheduleError{
                .kind = ScheduleErrorKind::ValidationError,
                .message = "MLQ validation failed: duplicate queue ID '" + q.id + "' detected.",
                .validation_result = std::nullopt
            };
        }

        if (q.algorithm == QueueAlgorithm::RoundRobin && q.internal_quantum <= 0) {
            return ScheduleError{
                .kind = ScheduleErrorKind::ValidationError,
                .message = "MLQ validation failed: Round Robin queue '" + q.id +
                           "' requires a positive internal time quantum (> 0).",
                .validation_result = std::nullopt
            };
        }

        if (workload.inter_queue_policy == InterQueuePolicy::RoundRobin) {
            if (q.inter_queue_quantum < 0) {
                return ScheduleError{
                    .kind = ScheduleErrorKind::ValidationError,
                    .message = "MLQ validation failed: queue '" + q.id +
                               "' specifies a negative inter-queue time quantum.",
                    .validation_result = std::nullopt
                };
            }
        }
    }

    if (workload.inter_queue_policy == InterQueuePolicy::RoundRobin &&
        workload.default_inter_queue_quantum <= 0) {
        return ScheduleError{
            .kind = ScheduleErrorKind::ValidationError,
            .message = "MLQ validation failed: default inter-queue time quantum must be strictly positive (> 0).",
            .validation_result = std::nullopt
        };
    }

    // Validate Underlying Processes
    std::vector<Process> procs;
    procs.reserve(workload.processes.size());
    for (const auto& binding : workload.processes) {
        procs.push_back(binding.process);
    }

    const ValidationResult proc_validation = ProcessValidator::validate(procs);
    if (!proc_validation.is_valid()) {
        return ScheduleError{
            .kind = ScheduleErrorKind::ValidationError,
            .message = "MLQ validation failed: workload contains invalid process definitions.",
            .validation_result = proc_validation
        };
    }

    // Validate Queue Bindings
    for (std::size_t i = 0; i < workload.processes.size(); ++i) {
        const auto& binding = workload.processes[i];
        if (ProcessValidator::is_blank(binding.queue_id)) {
            return ScheduleError{
                .kind = ScheduleErrorKind::ValidationError,
                .message = "MLQ validation failed: process '" + binding.process.id +
                           "' has an empty or blank queue assignment.",
                .validation_result = std::nullopt
            };
        }

        const std::string norm_qid = normalize_id(binding.queue_id);
        if (!seen_queue_ids.contains(norm_qid)) {
            return ScheduleError{
                .kind = ScheduleErrorKind::ValidationError,
                .message = "MLQ validation failed: process '" + binding.process.id +
                           "' references unknown queue ID '" + binding.queue_id + "'.",
                .validation_result = std::nullopt
            };
        }
    }

    // ------------------------------------------------------------------------
    // 2. Setup Simulation State
    // ------------------------------------------------------------------------
    const std::size_t num_procs = workload.processes.size();
    const std::size_t num_queues = workload.queues.size();

    // Map normalized queue ID to queue index
    auto get_queue_index = [&](std::string_view qid) -> std::size_t {
        const std::string norm = normalize_id(qid);
        for (std::size_t i = 0; i < num_queues; ++i) {
            if (normalize_id(workload.queues[i].id) == norm) {
                return i;
            }
        }
        return 0;
    };

    // Initialize per-process tracking and states in original input order
    std::vector<ProcessState> states;
    states.reserve(num_procs);
    std::vector<ProcessEntry> entries;
    entries.reserve(num_procs);

    for (std::size_t i = 0; i < num_procs; ++i) {
        const auto& binding = workload.processes[i];
        const std::size_t q_idx = get_queue_index(binding.queue_id);
        ProcessState state = ProcessState::from_process(binding.process);
        state.queue_id = workload.queues[q_idx].id;
        states.push_back(std::move(state));

        entries.push_back(ProcessEntry{
            .original_index = i,
            .process = &workload.processes[i].process,
            .queue_index = get_queue_index(binding.queue_id),
            .remaining_burst = binding.process.burst_time
        });
    }

    // Sort unarrived entries by arrival time, maintaining input stability
    std::vector<std::size_t> unarrived_indices(num_procs);
    for (std::size_t i = 0; i < num_procs; ++i) {
        unarrived_indices[i] = i;
    }
    std::stable_sort(unarrived_indices.begin(), unarrived_indices.end(),
                     [&](std::size_t a, std::size_t b) {
                         if (entries[a].process->arrival_time != entries[b].process->arrival_time) {
                             return entries[a].process->arrival_time < entries[b].process->arrival_time;
                         }
                         return entries[a].original_index < entries[b].original_index;
                     });

    // Initialize queue runtime states
    std::vector<QueueInternalState> queues;
    queues.reserve(num_queues);
    for (std::size_t i = 0; i < num_queues; ++i) {
        const auto& qconf = workload.queues[i];
        Time effective_iq = (qconf.inter_queue_quantum > 0)
            ? qconf.inter_queue_quantum
            : workload.default_inter_queue_quantum;

        queues.push_back(QueueInternalState{
            .config = qconf,
            .config_index = i,
            .effective_iq_quantum = effective_iq,
            .ready_deque = {},
            .ready_pool = {},
            .active_proc_idx = std::nullopt,
            .active_internal_q_remaining = qconf.internal_quantum
        });
    }

    std::vector<ExecutionSegment> timeline;
    Time current_time = 0;
    Time total_busy_time = 0;
    Time total_idle_time = 0;
    std::size_t next_unarrived_idx = 0;
    std::size_t completed_count = 0;

    // Inter-queue Round Robin state
    std::size_t iq_rr_current_queue = 0;
    Time iq_rr_quantum_remaining = queues[0].effective_iq_quantum;

    // Helper: Admit newly arrived processes up to current_time
    auto admit_arrivals = [&](Time target_time) {
        while (next_unarrived_idx < unarrived_indices.size()) {
            const std::size_t entry_idx = unarrived_indices[next_unarrived_idx];
            if (entries[entry_idx].process->arrival_time > target_time) {
                break;
            }
            next_unarrived_idx++;

            auto& q = queues[entries[entry_idx].queue_index];
            if (q.config.algorithm == QueueAlgorithm::Fcfs ||
                q.config.algorithm == QueueAlgorithm::RoundRobin) {
                q.ready_deque.push_back(entry_idx);
            } else {
                q.ready_pool.push_back(entry_idx);
            }
        }
    };

    // Helper: Select the active process within a queue according to its policy
    auto update_queue_active_process = [&](QueueInternalState& q) {
        switch (q.config.algorithm) {
            case QueueAlgorithm::Fcfs: {
                if (!q.active_proc_idx.has_value() && !q.ready_deque.empty()) {
                    q.active_proc_idx = q.ready_deque.front();
                    q.ready_deque.pop_front();
                }
                break;
            }
            case QueueAlgorithm::RoundRobin: {
                if (!q.active_proc_idx.has_value() && !q.ready_deque.empty()) {
                    q.active_proc_idx = q.ready_deque.front();
                    q.ready_deque.pop_front();
                    q.active_internal_q_remaining = q.config.internal_quantum;
                }
                break;
            }
            case QueueAlgorithm::Sjf: {
                // Non-preemptive within queue: if already executing, keep running
                if (!q.active_proc_idx.has_value() && !q.ready_pool.empty()) {
                    auto best_it = std::min_element(q.ready_pool.begin(), q.ready_pool.end(),
                        [&](std::size_t a, std::size_t b) {
                            if (entries[a].process->burst_time != entries[b].process->burst_time) {
                                return entries[a].process->burst_time < entries[b].process->burst_time;
                            }
                            if (entries[a].process->arrival_time != entries[b].process->arrival_time) {
                                return entries[a].process->arrival_time < entries[b].process->arrival_time;
                            }
                            return entries[a].original_index < entries[b].original_index;
                        });
                    q.active_proc_idx = *best_it;
                    q.ready_pool.erase(best_it);
                }
                break;
            }
            case QueueAlgorithm::NonPreemptivePriority: {
                // Non-preemptive within queue: if already executing, keep running
                if (!q.active_proc_idx.has_value() && !q.ready_pool.empty()) {
                    auto best_it = std::min_element(q.ready_pool.begin(), q.ready_pool.end(),
                        [&](std::size_t a, std::size_t b) {
                            if (entries[a].process->priority != entries[b].process->priority) {
                                return entries[a].process->priority < entries[b].process->priority;
                            }
                            if (entries[a].process->arrival_time != entries[b].process->arrival_time) {
                                return entries[a].process->arrival_time < entries[b].process->arrival_time;
                            }
                            return entries[a].original_index < entries[b].original_index;
                        });
                    q.active_proc_idx = *best_it;
                    q.ready_pool.erase(best_it);
                }
                break;
            }
            case QueueAlgorithm::Srtf: {
                // Preemptive within queue: compare with ready pool
                if (!q.ready_pool.empty()) {
                    auto best_it = std::min_element(q.ready_pool.begin(), q.ready_pool.end(),
                        [&](std::size_t a, std::size_t b) {
                            if (entries[a].remaining_burst != entries[b].remaining_burst) {
                                return entries[a].remaining_burst < entries[b].remaining_burst;
                            }
                            if (entries[a].process->arrival_time != entries[b].process->arrival_time) {
                                return entries[a].process->arrival_time < entries[b].process->arrival_time;
                            }
                            return entries[a].original_index < entries[b].original_index;
                        });

                    if (!q.active_proc_idx.has_value()) {
                        q.active_proc_idx = *best_it;
                        q.ready_pool.erase(best_it);
                    } else {
                        // Preemption check within queue: strictly smaller remaining burst
                        if (entries[*best_it].remaining_burst < entries[*q.active_proc_idx].remaining_burst) {
                            const std::size_t new_active = *best_it;
                            const std::size_t old_active = *q.active_proc_idx;
                            q.ready_pool.erase(best_it);
                            q.ready_pool.push_back(old_active);
                            q.active_proc_idx = new_active;
                        }
                    }
                }
                break;
            }
            case QueueAlgorithm::PreemptivePriority: {
                // Preemptive within queue: compare with ready pool
                if (!q.ready_pool.empty()) {
                    auto best_it = std::min_element(q.ready_pool.begin(), q.ready_pool.end(),
                        [&](std::size_t a, std::size_t b) {
                            if (entries[a].process->priority != entries[b].process->priority) {
                                return entries[a].process->priority < entries[b].process->priority;
                            }
                            if (entries[a].process->arrival_time != entries[b].process->arrival_time) {
                                return entries[a].process->arrival_time < entries[b].process->arrival_time;
                            }
                            return entries[a].original_index < entries[b].original_index;
                        });

                    if (!q.active_proc_idx.has_value()) {
                        q.active_proc_idx = *best_it;
                        q.ready_pool.erase(best_it);
                    } else {
                        // Preemption check within queue: strictly higher priority (smaller numerical value)
                        if (entries[*best_it].process->priority < entries[*q.active_proc_idx].process->priority) {
                            const std::size_t new_active = *best_it;
                            const std::size_t old_active = *q.active_proc_idx;
                            q.ready_pool.erase(best_it);
                            q.ready_pool.push_back(old_active);
                            q.active_proc_idx = new_active;
                        }
                    }
                }
                break;
            }
        }
    };

    // Helper: Select the active queue under the configured inter-queue policy
    auto select_active_queue = [&]() -> std::optional<std::size_t> {
        if (workload.inter_queue_policy == InterQueuePolicy::FixedPriorityPreemptive) {
            std::optional<std::size_t> best_q = std::nullopt;
            for (std::size_t i = 0; i < num_queues; ++i) {
                if (queues[i].has_ready_processes()) {
                    if (!best_q.has_value()) {
                        best_q = i;
                    } else {
                        // Priority comparison: smaller numerical value wins; tie-break by config_index
                        if (queues[i].config.priority < queues[*best_q].config.priority) {
                            best_q = i;
                        } else if (queues[i].config.priority == queues[*best_q].config.priority) {
                            if (queues[i].config_index < queues[*best_q].config_index) {
                                best_q = i;
                            }
                        }
                    }
                }
            }
            return best_q;
        } else {
            // InterQueuePolicy::RoundRobin
            // Check if current queue still has ready processes and remaining quantum
            if (queues[iq_rr_current_queue].has_ready_processes() && iq_rr_quantum_remaining > 0) {
                return iq_rr_current_queue;
            }

            // Otherwise, search for the next non-empty queue in cyclic order
            for (std::size_t step = 1; step <= num_queues; ++step) {
                const std::size_t candidate = (iq_rr_current_queue + step) % num_queues;
                if (queues[candidate].has_ready_processes()) {
                    iq_rr_current_queue = candidate;
                    iq_rr_quantum_remaining = queues[candidate].effective_iq_quantum;
                    return iq_rr_current_queue;
                }
            }

            // No queue has ready processes
            return std::nullopt;
        }
    };

    // ------------------------------------------------------------------------
    // 3. Event-Driven Simulation Loop
    // ------------------------------------------------------------------------
    admit_arrivals(current_time);

    while (completed_count < num_procs) {
        // Step A: Select active queue and update its internal active process
        auto active_q_opt = select_active_queue();

        // If no queue has ready processes, advance time to next arrival (idle interval)
        if (!active_q_opt.has_value()) {
            if (next_unarrived_idx < unarrived_indices.size()) {
                const Time next_arr = entries[unarrived_indices[next_unarrived_idx]].process->arrival_time;
                if (next_arr > current_time) {
                    const Time idle_duration = next_arr - current_time;
                    timeline.push_back(ExecutionSegment::idle(current_time, next_arr));

                    if (!safe_add(total_idle_time, idle_duration, total_idle_time)) {
                        return ScheduleError{
                            .kind = ScheduleErrorKind::ArithmeticOverflow,
                            .message = "Arithmetic overflow occurred while accumulating CPU idle time.",
                            .validation_result = std::nullopt
                        };
                    }
                    current_time = next_arr;
                }
                admit_arrivals(current_time);
                continue;
            } else {
                // All processes completed
                break;
            }
        }

        const std::size_t active_q_idx = *active_q_opt;
        auto& active_queue = queues[active_q_idx];

        update_queue_active_process(active_queue);
        if (!active_queue.active_proc_idx.has_value()) {
            // Queue has no dispatchable process right now
            continue;
        }

        const std::size_t active_p_idx = *active_queue.active_proc_idx;
        auto& active_entry = entries[active_p_idx];
        ProcessState& active_state = states[active_entry.original_index];

        // Mark initial response timestamp
        if (!active_state.start_time.has_value()) {
            active_state.start_time = current_time;
        }
        active_state.status = ProcessExecutionStatus::Running;

        // Step B: Calculate maximum continuous execution duration before next event
        Time max_duration = active_entry.remaining_burst;

        // Constraint 1: Internal Round Robin quantum
        if (active_queue.config.algorithm == QueueAlgorithm::RoundRobin) {
            max_duration = std::min(max_duration, active_queue.active_internal_q_remaining);
        }

        // Constraint 2: Inter-queue Round Robin quantum
        if (workload.inter_queue_policy == InterQueuePolicy::RoundRobin) {
            max_duration = std::min(max_duration, iq_rr_quantum_remaining);
        }

        // Constraint 3: Next arrival in the system
        if (next_unarrived_idx < unarrived_indices.size()) {
            const Time next_arr = entries[unarrived_indices[next_unarrived_idx]].process->arrival_time;
            if (next_arr > current_time) {
                const Time time_to_arr = next_arr - current_time;
                max_duration = std::min(max_duration, time_to_arr);
            }
        }

        // Advance simulation clock
        Time slice_end = 0;
        if (!safe_add(current_time, max_duration, slice_end)) {
            return ScheduleError{
                .kind = ScheduleErrorKind::ArithmeticOverflow,
                .message = "Arithmetic overflow occurred while advancing simulation clock for process '" +
                           active_entry.process->id + "'.",
                .validation_result = std::nullopt
            };
        }

        if (!safe_add(total_busy_time, max_duration, total_busy_time)) {
            return ScheduleError{
                .kind = ScheduleErrorKind::ArithmeticOverflow,
                .message = "Arithmetic overflow occurred while accumulating CPU busy time.",
                .validation_result = std::nullopt
            };
        }

        // Record execution timeline segment
        append_normalized_segment(timeline, active_entry.process->id, current_time, slice_end);

        // Update remaining times
        active_entry.remaining_burst -= max_duration;
        if (active_queue.config.algorithm == QueueAlgorithm::RoundRobin) {
            active_queue.active_internal_q_remaining -= max_duration;
        }
        if (workload.inter_queue_policy == InterQueuePolicy::RoundRobin) {
            iq_rr_quantum_remaining -= max_duration;
        }
        current_time = slice_end;

        // Step C: Admit all processes that arrived at or before the new timestamp
        admit_arrivals(current_time);

        // Step D: Process completion or quantum expirations
        if (active_entry.remaining_burst == 0) {
            // Process completed!
            active_state.completion_time = current_time;
            active_state.turnaround_time = current_time - active_entry.process->arrival_time;
            active_state.waiting_time = active_state.turnaround_time - active_entry.process->burst_time;
            active_state.response_time = active_state.start_time.value() - active_entry.process->arrival_time;
            active_state.remaining_time = 0;
            active_state.status = ProcessExecutionStatus::Completed;

            active_queue.active_proc_idx = std::nullopt;
            completed_count++;
        } else if (active_queue.config.algorithm == QueueAlgorithm::RoundRobin &&
                   active_queue.active_internal_q_remaining == 0) {
            // Internal RR quantum expired: requeue process to back of its queue
            active_queue.ready_deque.push_back(active_p_idx);
            active_queue.active_proc_idx = std::nullopt;
        }

        // Inter-queue Round Robin quantum expiration
        if (workload.inter_queue_policy == InterQueuePolicy::RoundRobin) {
            if (iq_rr_quantum_remaining == 0 || !active_queue.has_ready_processes()) {
                // Queue loses CPU turn, move to next queue on next iteration
                iq_rr_quantum_remaining = 0;
            }
        }
    }

    // ------------------------------------------------------------------------
    // 4. Compute Summary Metrics
    // ------------------------------------------------------------------------
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

