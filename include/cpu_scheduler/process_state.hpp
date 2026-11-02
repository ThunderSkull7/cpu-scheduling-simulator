#pragma once

#include "cpu_scheduler/process.hpp"

#include <iosfwd>
#include <optional>
#include <string>

namespace cpu_scheduler {

/// Lifecycle states of a process during execution.
enum class ProcessExecutionStatus {
    NotStarted,
    Running,
    Completed
};

/**
 * @brief Represents the dynamic simulation state and calculated metrics of a process.
 *
 * Strictly decoupled from the immutable input `Process` specification.
 */
struct ProcessState {
    std::string process_id{};
    Time arrival_time{0};
    Time burst_time{0};
    int priority{0};

    Time remaining_time{0};
    std::optional<Time> start_time{std::nullopt};
    Time completion_time{0};
    Time turnaround_time{0};
    Time waiting_time{0};
    Time response_time{0};
    ProcessExecutionStatus status{ProcessExecutionStatus::NotStarted};

    constexpr ProcessState() = default;

    /// Constructs a state initialized from an input Process specification.
    static ProcessState from_process(const Process& process);

    [[nodiscard]] bool operator==(const ProcessState& other) const = default;
};

std::ostream& operator<<(std::ostream& os, const ProcessState& state);
std::ostream& operator<<(std::ostream& os, ProcessExecutionStatus status);

} // namespace cpu_scheduler
