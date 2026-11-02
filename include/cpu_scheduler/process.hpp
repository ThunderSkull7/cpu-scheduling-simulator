#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

namespace cpu_scheduler {

/// Alias for time points and durations in the simulator.
/// Uses 64-bit signed integer to prevent overflow issues.
using Time = std::int64_t;

/**
 * @brief Represents an immutable specification of a CPU process workload.
 *
 * This data model strictly holds input workload definitions (arrival time,
 * original burst time, priority, and identifier). Mutable scheduling states
 * (such as remaining execution time) and computed performance metrics
 * (such as turnaround time, waiting time, response time) must not be stored here,
 * adhering to clean separation of concerns.
 */
struct Process {
    std::string id{};
    Time arrival_time{0};
    Time burst_time{0};
    int priority{0};

    constexpr Process() = default;

    Process(std::string process_id, Time arrival, Time burst, int prio = 0)
        : id(std::move(process_id)),
          arrival_time(arrival),
          burst_time(burst),
          priority(prio) {}

    [[nodiscard]] bool operator==(const Process& other) const = default;
};

std::ostream& operator<<(std::ostream& os, const Process& process);

} // namespace cpu_scheduler
