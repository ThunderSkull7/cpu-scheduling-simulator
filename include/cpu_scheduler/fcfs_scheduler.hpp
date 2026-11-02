#pragma once

#include "cpu_scheduler/process.hpp"
#include "cpu_scheduler/scheduling_result.hpp"

#include <span>
#include <vector>

namespace cpu_scheduler {

/**
 * @brief Non-preemptive First Come, First Served (FCFS) CPU scheduler.
 *
 * Dispatches processes in order of arrival time. Breaks simultaneous arrival ties
 * by preserving the original input order. Generates explicit CPU idle segments
 * when no processes are ready.
 */
class FcfsScheduler {
public:
    constexpr FcfsScheduler() = default;

    /**
     * @brief Executes the FCFS scheduling algorithm over the provided workload.
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
        return FcfsScheduler{}.schedule(processes);
    }

    [[nodiscard]] static ScheduleOutcome run(const std::vector<Process>& processes) {
        return FcfsScheduler{}.schedule(processes);
    }
};

} // namespace cpu_scheduler
