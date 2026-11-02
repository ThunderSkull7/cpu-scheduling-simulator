#include "cpu_scheduler/mlfq_scheduler.hpp"
#include "cpu_scheduler/execution_segment.hpp"
#include "cpu_scheduler/process_state.hpp"
#include "cpu_scheduler/process_validator.hpp"

#include <algorithm>
#include <deque>
#include <limits>
#include <string>
#include <vector>

namespace cpu_scheduler {
namespace {

/// Helper to safely add two Time values, detecting signed 64-bit integer overflow.
[[nodiscard]] constexpr bool safe_add(Time a, Time b, Time& result) noexcept {
    if (b > 0 && a > (std::numeric_limits<Time>::max() - b)) {
        return false;
    }
    if (b < 0 && a < (std::numeric_limits<Time>::min() - b)) {
        return false;
    }
    result = a + b;
    return true;
}

/// Internal tracking state for an individual process in MLFQ simulation.
struct MlfqProcessEntry {
    std::size_t original_index{0};
    const Process* process{nullptr};
    Time remaining_burst{0};
    std::size_t current_level{0};
    Time current_level_allotment_remaining{0};
    Time current_quantum_remaining{0};
};

} // namespace

ScheduleOutcome MlfqScheduler::schedule(std::span<const Process> processes) const {
    // ------------------------------------------------------------------------
    // 1. Configuration Validation
    // ------------------------------------------------------------------------
    if (config_.levels.size() < 2) {
        return ScheduleError{
            .kind = ScheduleErrorKind::ValidationError,
            .message = "MLFQ validation failed: configuration must define at least two queue levels.",
            .validation_result = std::nullopt
        };
    }

    for (std::size_t i = 0; i < config_.levels.size(); ++i) {
        const auto& level = config_.levels[i];
        if (level.time_quantum <= 0) {
            return ScheduleError{
                .kind = ScheduleErrorKind::ValidationError,
                .message = "MLFQ validation failed: queue level " + std::to_string(i) +
                           " has non-positive time quantum (" + std::to_string(level.time_quantum) + ").",
                .validation_result = std::nullopt
            };
        }
        if (level.effective_allotment() <= 0) {
            return ScheduleError{
                .kind = ScheduleErrorKind::ValidationError,
                .message = "MLFQ validation failed: queue level " + std::to_string(i) +
                           " has non-positive CPU allotment (" + std::to_string(level.effective_allotment()) + ").",
                .validation_result = std::nullopt
            };
        }
    }

    if (config_.boost_interval.has_value() && *config_.boost_interval < 0) {
        return ScheduleError{
            .kind = ScheduleErrorKind::ValidationError,
            .message = "MLFQ validation failed: boost interval cannot be negative.",
            .validation_result = std::nullopt
        };
    }

    // ------------------------------------------------------------------------
    // 2. Workload Validation
    // ------------------------------------------------------------------------
    const ValidationResult validation = ProcessValidator::validate(processes);
    if (!validation.is_valid()) {
        return ScheduleError{
            .kind = ScheduleErrorKind::ValidationError,
            .message = "MLFQ validation failed: workload contains invalid inputs.",
            .validation_result = validation
        };
    }

    const std::size_t num_procs = processes.size();
    const std::size_t num_levels = config_.levels.size();

    // ------------------------------------------------------------------------
    // 3. Setup Simulation State
    // ------------------------------------------------------------------------
    std::vector<ProcessState> states;
    states.reserve(num_procs);
    std::vector<MlfqProcessEntry> entries;
    entries.reserve(num_procs);

    for (std::size_t i = 0; i < num_procs; ++i) {
        ProcessState state = ProcessState::from_process(processes[i]);
        state.queue_id = "Q0";
        states.push_back(std::move(state));

        entries.push_back(MlfqProcessEntry{
            .original_index = i,
            .process = &processes[i],
            .remaining_burst = processes[i].burst_time,
            .current_level = 0,
            .current_level_allotment_remaining = config_.levels[0].effective_allotment(),
            .current_quantum_remaining = config_.levels[0].time_quantum
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

    // Ready queues for each priority level
    std::vector<std::deque<std::size_t>> queues(num_levels);

    std::vector<ExecutionSegment> timeline;
    Time current_time = 0;
    Time total_busy_time = 0;
    Time total_idle_time = 0;
    std::size_t next_unarrived_idx = 0;
    std::size_t completed_count = 0;

    // Track next scheduled priority boost timestamp
    Time next_boost_time = (config_.is_boost_enabled()) ? *config_.boost_interval : std::numeric_limits<Time>::max();

    // Helper: Admit newly arrived processes up to target_time into Q0
    auto admit_arrivals = [&](Time target_time) {
        while (next_unarrived_idx < unarrived_indices.size()) {
            const std::size_t entry_idx = unarrived_indices[next_unarrived_idx];
            if (entries[entry_idx].process->arrival_time > target_time) {
                break;
            }
            next_unarrived_idx++;

            entries[entry_idx].current_level = 0;
            entries[entry_idx].current_level_allotment_remaining = config_.levels[0].effective_allotment();
            entries[entry_idx].current_quantum_remaining = config_.levels[0].time_quantum;
            states[entry_idx].queue_id = "Q0";
            queues[0].push_back(entry_idx);
        }
    };

    // Helper: Apply priority boost (promotes all unfinished processes to Q0)
    auto apply_priority_boost = [&]() {
        std::vector<std::size_t> unfinished;
        unfinished.reserve(num_procs - completed_count);

        for (std::size_t l = 0; l < num_levels; ++l) {
            for (std::size_t idx : queues[l]) {
                unfinished.push_back(idx);
            }
            queues[l].clear();
        }

        for (std::size_t idx : unfinished) {
            entries[idx].current_level = 0;
            entries[idx].current_level_allotment_remaining = config_.levels[0].effective_allotment();
            entries[idx].current_quantum_remaining = config_.levels[0].time_quantum;
            states[idx].queue_id = "Q0";
        }

        // Deterministic ordering: 1. Earlier arrival time, 2. Earlier workload input order
        std::stable_sort(unfinished.begin(), unfinished.end(), [&](std::size_t a, std::size_t b) {
            if (entries[a].process->arrival_time != entries[b].process->arrival_time) {
                return entries[a].process->arrival_time < entries[b].process->arrival_time;
            }
            return entries[a].original_index < entries[b].original_index;
        });

        for (std::size_t idx : unfinished) {
            queues[0].push_back(idx);
        }
    };

    // ------------------------------------------------------------------------
    // 4. Event-Driven Simulation Loop
    // ------------------------------------------------------------------------
    admit_arrivals(current_time);

    while (completed_count < num_procs) {
        // Step A: Find the highest-priority non-empty queue
        std::optional<std::size_t> active_level_opt = std::nullopt;
        for (std::size_t l = 0; l < num_levels; ++l) {
            if (!queues[l].empty()) {
                active_level_opt = l;
                break;
            }
        }

        // If all queues are empty, advance time to next event (arrival or boost)
        if (!active_level_opt.has_value()) {
            Time next_event_time = std::numeric_limits<Time>::max();
            if (next_unarrived_idx < unarrived_indices.size()) {
                next_event_time = entries[unarrived_indices[next_unarrived_idx]].process->arrival_time;
            }
            if (config_.is_boost_enabled() && next_boost_time < next_event_time) {
                next_event_time = next_boost_time;
            }

            if (next_event_time < std::numeric_limits<Time>::max() && next_event_time > current_time) {
                const Time idle_duration = next_event_time - current_time;
                timeline.push_back(ExecutionSegment::idle(current_time, next_event_time));

                if (!safe_add(total_idle_time, idle_duration, total_idle_time)) {
                    return ScheduleError{
                        .kind = ScheduleErrorKind::ArithmeticOverflow,
                        .message = "Arithmetic overflow occurred while accumulating CPU idle time.",
                        .validation_result = std::nullopt
                    };
                }
                current_time = next_event_time;
            }

            // Admit arrivals and/or apply boost at new current_time
            admit_arrivals(current_time);

            if (config_.is_boost_enabled() && current_time >= next_boost_time) {
                apply_priority_boost();
                while (next_boost_time <= current_time) {
                    if (!safe_add(next_boost_time, *config_.boost_interval, next_boost_time)) {
                        next_boost_time = std::numeric_limits<Time>::max();
                        break;
                    }
                }
            }
            continue;
        }

        const std::size_t active_level = *active_level_opt;
        const std::size_t active_idx = queues[active_level].front();
        queues[active_level].pop_front();

        auto& active_entry = entries[active_idx];
        ProcessState& active_state = states[active_entry.original_index];

        // Mark initial response timestamp
        if (!active_state.start_time.has_value()) {
            active_state.start_time = current_time;
        }
        active_state.status = ProcessExecutionStatus::Running;
        active_state.queue_id = "Q" + std::to_string(active_level);

        // Step B: Calculate maximum execution slice before next event
        Time max_duration = active_entry.remaining_burst;

        // Constraint 1: Current quantum remaining for this level
        max_duration = std::min(max_duration, active_entry.current_quantum_remaining);

        // Constraint 2: Current level allotment remaining
        max_duration = std::min(max_duration, active_entry.current_level_allotment_remaining);

        // Constraint 3: Preemption by higher-priority queue arrival (all arrivals enter Q0)
        if (active_level > 0 && next_unarrived_idx < unarrived_indices.size()) {
            const Time next_arr = entries[unarrived_indices[next_unarrived_idx]].process->arrival_time;
            if (next_arr > current_time) {
                max_duration = std::min(max_duration, next_arr - current_time);
            }
        }

        // Constraint 4: Priority boost event
        if (config_.is_boost_enabled() && next_boost_time > current_time) {
            max_duration = std::min(max_duration, next_boost_time - current_time);
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

        // Record execution segment
        timeline.push_back(ExecutionSegment::process(active_entry.process->id, current_time, slice_end));

        // Deduct progress
        active_entry.remaining_burst -= max_duration;
        active_entry.current_level_allotment_remaining -= max_duration;
        active_entry.current_quantum_remaining -= max_duration;
        current_time = slice_end;

        // Step C: Event ordering at current_time
        // 1. Process completion
        bool completed_now = false;
        if (active_entry.remaining_burst == 0) {
            completed_now = true;
            active_state.completion_time = current_time;
            active_state.turnaround_time = current_time - active_entry.process->arrival_time;
            active_state.waiting_time = active_state.turnaround_time - active_entry.process->burst_time;
            active_state.response_time = active_state.start_time.value() - active_entry.process->arrival_time;
            active_state.remaining_time = 0;
            active_state.status = ProcessExecutionStatus::Completed;
            completed_count++;
        }

        // 2. Admit all newly arrived processes into Q0
        admit_arrivals(current_time);

        // 3. Priority boost event
        if (config_.is_boost_enabled() && current_time >= next_boost_time) {
            // If active process is still unfinished, temporarily return it to its queue so boost collects it
            if (!completed_now) {
                queues[active_level].push_front(active_idx);
            }

            apply_priority_boost();

            while (next_boost_time <= current_time) {
                if (!safe_add(next_boost_time, *config_.boost_interval, next_boost_time)) {
                    next_boost_time = std::numeric_limits<Time>::max();
                    break;
                }
            }
        } else if (!completed_now) {
            // 4. Resolve allotment expiration or preemption
            if (active_entry.current_level_allotment_remaining == 0) {
                // Demote to next lower level if not already at bottom level
                if (active_entry.current_level + 1 < num_levels) {
                    active_entry.current_level++;
                }
                active_entry.current_level_allotment_remaining =
                    config_.levels[active_entry.current_level].effective_allotment();
                active_entry.current_quantum_remaining =
                    config_.levels[active_entry.current_level].time_quantum;
                active_state.queue_id = "Q" + std::to_string(active_entry.current_level);

                // Append to back of destination queue
                queues[active_entry.current_level].push_back(active_idx);
            } else if (active_entry.current_quantum_remaining == 0) {
                // Quantum expired within current level, but allotment remains
                active_entry.current_quantum_remaining =
                    config_.levels[active_entry.current_level].time_quantum;
                queues[active_entry.current_level].push_back(active_idx);
            } else {
                // Preempted by a higher-priority arrival before exhausting its quantum;
                // keep at front of its current level to resume when CPU returns to this queue
                queues[active_level].push_front(active_idx);
            }
        }
    }

    // ------------------------------------------------------------------------
    // 5. Compute Summary Metrics
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
