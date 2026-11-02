#pragma once

#include "cpu_scheduler/process.hpp"
#include "cpu_scheduler/scheduling_result.hpp"

#include <span>
#include <vector>

namespace cpu_scheduler {

/**
 * @brief Preemptive Shortest Remaining Time First (SRTF) CPU scheduler.
 *
 * SRTF is the preemptive variant of Shortest Job First (SJF). At each scheduling
 * decision point, the scheduler allocates the CPU to the process with the
 * smallest remaining execution time among all processes that have arrived and
 * are not yet completed.
 *
 * Preemption Rule:
 *  - The currently executing process is preempted IF AND ONLY IF another ready
 *    process has a STRICTLY smaller remaining burst time.
 *  - If a newly arrived or ready process has an equal remaining time to the
 *    currently running process, no preemption occurs. The running process retains
 *    the CPU to avoid unnecessary context switches.
 *
 * Deterministic Tie-Breaking Rules:
 *  1. Shortest remaining execution time.
 *  2. If remaining times are equal, earlier arrival time wins.
 *  3. If both remaining times and arrival times are equal, the process with the
 *     lower original input workload sequence index wins (preserving input stability).
 *
 * Simulation Engine:
 *  - Implemented as an event-driven simulation where clock advancement jumps
 *    between discrete scheduling events (process arrivals and slice completions),
 *    rather than stepping through individual time units.
 *  - Time complexity: O(n log n), where n is the number of processes.
 *  - Space complexity: O(n).
 */
class SrtfScheduler {
public:
    constexpr SrtfScheduler() = default;

    /**
     * @brief Executes the preemptive SRTF scheduling algorithm.
     *
     * Validates workload integrity using ProcessValidator before simulating.
     *
     * @param processes Input collection of processes.
     * @return ScheduleOutcome containing the SchedulingResult on success,
     *         or a detailed ScheduleError on validation or arithmetic overflow failure.
     */
    [[nodiscard]] ScheduleOutcome schedule(std::span<const Process> processes) const;

    [[nodiscard]] ScheduleOutcome schedule(const std::vector<Process>& processes) const {
        return schedule(std::span<const Process>(processes));
    }

    /// Static convenience method
    [[nodiscard]] static ScheduleOutcome run(std::span<const Process> processes) {
        return SrtfScheduler{}.schedule(processes);
    }

    [[nodiscard]] static ScheduleOutcome run(const std::vector<Process>& processes) {
        return SrtfScheduler{}.schedule(processes);
    }
};

} // namespace cpu_scheduler
