#pragma once

#include "cpu_scheduler/process.hpp"
#include "cpu_scheduler/scheduling_result.hpp"

#include <span>
#include <vector>

namespace cpu_scheduler {

/**
 * @brief Non-preemptive Shortest Job First (SJF) CPU scheduler.
 *
 * Dynamically selects the ready process with the shortest CPU burst time
 * among all processes that have already arrived.
 *
 * Tie-breaking rules:
 *  1. Shortest burst time.
 *  2. If burst times are equal, earlier arrival time.
 *  3. If burst and arrival times are equal, preserve original input order.
 *
 * Once selected, a process runs to completion without preemption.
 * Explicit CPU idle segments are recorded when no process has arrived.
 */
class SjfScheduler {
public:
    constexpr SjfScheduler() = default;

    /**
     * @brief Executes the non-preemptive SJF scheduling algorithm.
     *
     * Validates workload integrity using ProcessValidator before simulating.
     *
     * @param processes Input collection of processes.
     * @return ScheduleOutcome containing the SchedulingResult on success,
     *         or a detailed ScheduleError on validation or overflow failure.
     */
    [[nodiscard]] ScheduleOutcome schedule(std::span<const Process> processes) const;

    [[nodiscard]] ScheduleOutcome schedule(const std::vector<Process>& processes) const {
        return schedule(std::span<const Process>(processes));
    }

    /// Static convenience method
    [[nodiscard]] static ScheduleOutcome run(std::span<const Process> processes) {
        return SjfScheduler{}.schedule(processes);
    }

    [[nodiscard]] static ScheduleOutcome run(const std::vector<Process>& processes) {
        return SjfScheduler{}.schedule(processes);
    }
};

} // namespace cpu_scheduler
