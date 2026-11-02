#pragma once

#include "cpu_scheduler/execution_segment.hpp"
#include "cpu_scheduler/process_state.hpp"
#include "cpu_scheduler/process_validator.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cpu_scheduler {

/// Aggregated system-wide scheduling performance metrics.
struct SchedulingMetrics {
    Time total_execution_time{0};
    Time cpu_busy_time{0};
    Time cpu_idle_time{0};
    double cpu_utilization{0.0};
    double average_waiting_time{0.0};
    double average_turnaround_time{0.0};
    double average_response_time{0.0};

    [[nodiscard]] bool operator==(const SchedulingMetrics& other) const = default;
};

/**
 * @brief Encapsulates the complete result of a scheduling simulation.
 */
class SchedulingResult {
public:
    SchedulingResult() = default;

    SchedulingResult(std::vector<ExecutionSegment> timeline,
                     std::vector<ProcessState> processes,
                     SchedulingMetrics metrics)
        : timeline_(std::move(timeline)),
          processes_(std::move(processes)),
          metrics_(metrics) {}

    [[nodiscard]] const std::vector<ExecutionSegment>& timeline() const noexcept {
        return timeline_;
    }

    [[nodiscard]] const std::vector<ProcessState>& processes() const noexcept {
        return processes_;
    }

    [[nodiscard]] const SchedulingMetrics& metrics() const noexcept {
        return metrics_;
    }

    [[nodiscard]] Time total_execution_time() const noexcept {
        return metrics_.total_execution_time;
    }

    [[nodiscard]] Time cpu_busy_time() const noexcept {
        return metrics_.cpu_busy_time;
    }

    [[nodiscard]] Time cpu_idle_time() const noexcept {
        return metrics_.cpu_idle_time;
    }

    [[nodiscard]] double cpu_utilization() const noexcept {
        return metrics_.cpu_utilization;
    }

    [[nodiscard]] double average_waiting_time() const noexcept {
        return metrics_.average_waiting_time;
    }

    [[nodiscard]] double average_turnaround_time() const noexcept {
        return metrics_.average_turnaround_time;
    }

    [[nodiscard]] double average_response_time() const noexcept {
        return metrics_.average_response_time;
    }

    [[nodiscard]] const ProcessState* find_process_state(std::string_view process_id) const noexcept;

private:
    std::vector<ExecutionSegment> timeline_{};
    std::vector<ProcessState> processes_{};
    SchedulingMetrics metrics_{};
};

/// Category of scheduling failure.
enum class ScheduleErrorKind {
    ValidationError,
    ArithmeticOverflow
};

/// Structured representation of a scheduling failure.
struct ScheduleError {
    ScheduleErrorKind kind{ScheduleErrorKind::ValidationError};
    std::string message;
    std::optional<ValidationResult> validation_result{std::nullopt};

    [[nodiscard]] bool operator==(const ScheduleError& other) const = default;
};

/**
 * @brief Discriminant union representing either a successful SchedulingResult
 *        or a detailed ScheduleError.
 */
class ScheduleOutcome {
public:
    /* implicit */ ScheduleOutcome(SchedulingResult result);
    /* implicit */ ScheduleOutcome(ScheduleError error);

    [[nodiscard]] bool has_value() const noexcept {
        return has_value_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value_;
    }

    [[nodiscard]] const SchedulingResult& value() const;
    [[nodiscard]] const SchedulingResult& operator*() const {
        return value();
    }
    [[nodiscard]] const SchedulingResult* operator->() const {
        return &value();
    }

    [[nodiscard]] const ScheduleError& error() const;

    [[nodiscard]] bool is_validation_error() const noexcept {
        return !has_value_ && error_.has_value() &&
               error_->kind == ScheduleErrorKind::ValidationError;
    }

    [[nodiscard]] bool is_overflow_error() const noexcept {
        return !has_value_ && error_.has_value() &&
               error_->kind == ScheduleErrorKind::ArithmeticOverflow;
    }

    [[nodiscard]] std::string error_message() const;

private:
    bool has_value_{false};
    std::optional<SchedulingResult> result_{std::nullopt};
    std::optional<ScheduleError> error_{std::nullopt};
};

} // namespace cpu_scheduler
