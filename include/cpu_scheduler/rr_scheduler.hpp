#pragma once

#include "cpu_scheduler/process.hpp"
#include "cpu_scheduler/scheduling_result.hpp"

#include <span>
#include <vector>

namespace cpu_scheduler {

/**
 * @brief Preemptive Round Robin (RR) CPU scheduler.
 *
 * Round Robin is a preemptive scheduling algorithm that assigns each ready process
 * a fixed maximum continuous CPU slice called the time quantum.
 *
 * Core Scheduling Rules:
 *  1. Processes are managed in a strict FIFO ready queue.
 *  2. Eligible processes (arrival_time <= current_time) enter the ready queue in
 *     ascending arrival-time order. For simultaneous arrivals, original workload input
 *     sequence is strictly preserved.
 *  3. The process at the head of the ready queue receives the CPU and executes for:
 *     slice_duration = min(time_quantum, remaining_burst_time).
 *  4. Quantum Expiration & Simultaneous Arrivals:
 *     When a running process finishes its allotted quantum:
 *      a. The simulation clock is advanced by slice_duration.
 *      b. All processes arriving at or before that exact timestamp are admitted into
 *         the ready queue BEFORE the expired process is requeued.
 *      c. If the active process still has remaining execution time, it is placed at the
 *         tail of the ready queue.
 *      d. The next process at the front of the ready queue is dispatched.
 *  5. Process Completion:
 *     If a process finishes its burst at or before quantum expiration, it is marked
 *     completed, retired permanently, and not requeued.
 *  6. No Unnecessary Context Switching / Timeline Normalization:
 *     If a process reaches its quantum boundary and no other process is waiting in
 *     the ready queue, it retains the CPU. Adjacent execution segments for the same
 *     process are normalized into a continuous timeline segment.
 *  7. CPU Idle Fast-Forwarding:
 *     If the ready queue is empty, the simulation clock jumps directly to the next
 *     process arrival, recording an explicit idle segment.
 *
 * Complexity:
 *  - Time Complexity: O(n log n) primarily due to initial arrival sorting.
 *  - Space Complexity: O(n) for the ready queue and state tracking.
 */
class RrScheduler {
public:
    /// Default constructor with standard quantum of 2
    constexpr RrScheduler() noexcept : time_quantum_(2) {}

    /// Explicit constructor with configurable time quantum
    explicit constexpr RrScheduler(Time time_quantum) noexcept : time_quantum_(time_quantum) {}

    /**
     * @brief Gets the configured time quantum.
     */
    [[nodiscard]] constexpr Time time_quantum() const noexcept {
        return time_quantum_;
    }

    /**
     * @brief Executes the Round Robin scheduling algorithm using the configured quantum.
     *
     * @param processes Input workload collection.
     * @return ScheduleOutcome containing SchedulingResult on success, or ScheduleError on failure.
     */
    [[nodiscard]] ScheduleOutcome schedule(std::span<const Process> processes) const;

    [[nodiscard]] ScheduleOutcome schedule(const std::vector<Process>& processes) const {
        return schedule(std::span<const Process>(processes));
    }

    /**
     * @brief Executes the Round Robin scheduling algorithm with an explicit time quantum override.
     *
     * @param processes Input workload collection.
     * @param time_quantum Positive time quantum value.
     * @return ScheduleOutcome containing SchedulingResult on success, or ScheduleError on failure.
     */
    [[nodiscard]] ScheduleOutcome schedule(std::span<const Process> processes, Time time_quantum) const;

    [[nodiscard]] ScheduleOutcome schedule(const std::vector<Process>& processes, Time time_quantum) const {
        return schedule(std::span<const Process>(processes), time_quantum);
    }

    /// Static convenience methods
    [[nodiscard]] static ScheduleOutcome run(std::span<const Process> processes, Time time_quantum) {
        return RrScheduler{time_quantum}.schedule(processes);
    }

    [[nodiscard]] static ScheduleOutcome run(const std::vector<Process>& processes, Time time_quantum) {
        return RrScheduler{time_quantum}.schedule(processes);
    }

private:
    Time time_quantum_{2};
};

} // namespace cpu_scheduler
