#pragma once

#include "cpu_scheduler/process.hpp"
#include "cpu_scheduler/scheduling_result.hpp"

#include <span>
#include <vector>

namespace cpu_scheduler {

/**
 * @brief Non-preemptive Priority CPU scheduler.
 *
 * Dynamically selects the ready process with the highest priority (numerically smallest
 * priority value) among all processes that have already arrived.
 *
 * Priority Direction:
 *  - A smaller numerical priority value represents a strictly higher scheduling priority.
 *    For example, priority 1 has higher priority than priority 2, and priority -5 has higher
 *    priority than priority 0.
 *
 * Deterministic Tie-Breaking Rules:
 *  1. Highest priority (smallest numerical priority value).
 *  2. If priorities are equal, earlier arrival time wins.
 *  3. If both priority and arrival time are equal, original input workload sequence index wins.
 *
 * Execution Policy:
 *  - Non-preemptive: Once a process begins execution, it runs to completion without interruption.
 *  - Newly arrived processes with higher priority must wait until the currently running process finishes.
 *  - Explicit CPU idle segments are recorded when no process has arrived.
 */
class NonPreemptivePriorityScheduler {
public:
    constexpr NonPreemptivePriorityScheduler() = default;

    /**
     * @brief Executes non-preemptive priority scheduling on the provided workload.
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
        return NonPreemptivePriorityScheduler{}.schedule(processes);
    }

    [[nodiscard]] static ScheduleOutcome run(const std::vector<Process>& processes) {
        return NonPreemptivePriorityScheduler{}.schedule(processes);
    }
};

/**
 * @brief Preemptive Priority CPU scheduler.
 *
 * Dynamically allocates the CPU to the highest-priority process (numerically smallest priority
 * value) among all processes that have arrived and are not yet completed.
 *
 * Priority Direction:
 *  - A smaller numerical priority value represents a strictly higher scheduling priority.
 *
 * Preemption Rules:
 *  - The currently executing process is preempted IF AND ONLY IF a newly arrived or ready process
 *    has a STRICTLY higher priority (i.e., new_process.priority < running_process.priority).
 *  - Equal-priority arrivals MUST NOT preempt the currently running process; the running process
 *    retains the CPU to prevent unnecessary context switches.
 *  - Lower-priority arrivals MUST NOT preempt the running process.
 *  - When preempted, the active process's remaining burst time is preserved and it returns to the
 *    ready pool.
 *
 * Deterministic Tie-Breaking Rules (among waiting ready processes):
 *  1. Highest priority (smallest numerical priority value).
 *  2. If priorities are equal, earlier arrival time wins.
 *  3. If both priority and arrival time are equal, original input workload sequence index wins.
 *
 * Simulation Engine:
 *  - Event-driven discrete simulation jumping directly between critical scheduling events:
 *    next arrival, active process completion, or higher-priority arrival preemption.
 *  - Time complexity: O(n log n).
 *  - Space complexity: O(n).
 */
class PreemptivePriorityScheduler {
public:
    constexpr PreemptivePriorityScheduler() = default;

    /**
     * @brief Executes preemptive priority scheduling on the provided workload.
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
        return PreemptivePriorityScheduler{}.schedule(processes);
    }

    [[nodiscard]] static ScheduleOutcome run(const std::vector<Process>& processes) {
        return PreemptivePriorityScheduler{}.schedule(processes);
    }
};

} // namespace cpu_scheduler
