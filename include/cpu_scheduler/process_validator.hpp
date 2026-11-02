#pragma once

#include "cpu_scheduler/process.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cpu_scheduler {

/// Specific failure categories for workload validation.
enum class ValidationErrorCode {
    EmptyWorkload,
    EmptyProcessId,
    DuplicateProcessId,
    NegativeArrivalTime,
    NonPositiveBurstTime
};

/// Represents an individual validation error in a workload.
struct ValidationError {
    ValidationErrorCode code;
    std::string message;
    std::optional<std::string> process_id{};
    std::optional<std::size_t> process_index{};

    [[nodiscard]] bool operator==(const ValidationError& other) const = default;
};

/// Encapsulates the complete outcome of workload validation.
class ValidationResult {
public:
    ValidationResult() = default;

    explicit ValidationResult(std::vector<ValidationError> errors)
        : errors_(std::move(errors)) {}

    [[nodiscard]] bool is_valid() const noexcept {
        return errors_.empty();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return is_valid();
    }

    [[nodiscard]] const std::vector<ValidationError>& errors() const noexcept {
        return errors_;
    }

    [[nodiscard]] std::size_t error_count() const noexcept {
        return errors_.size();
    }

    /// Generates a human-readable multi-line summary of all detected errors.
    [[nodiscard]] std::string summary() const;

private:
    std::vector<ValidationError> errors_{};
};

/// Responsible for verifying process workloads before simulation.
class ProcessValidator {
public:
    ProcessValidator() = delete;

    /**
     * @brief Validates an input workload.
     *
     * Checks:
     *  1. At least one process is present.
     *  2. Process IDs are non-empty and non-whitespace.
     *  3. Process IDs are unique (case-insensitive and ignoring surrounding whitespace).
     *  4. Arrival times are non-negative.
     *  5. Burst times are strictly positive (> 0).
     *
     * Does not modify input data. Accumulates all detected errors.
     *
     * @param processes Collection of processes to validate.
     * @return ValidationResult containing any detected errors.
     */
    [[nodiscard]] static ValidationResult validate(std::span<const Process> processes);

    [[nodiscard]] static ValidationResult validate(const std::vector<Process>& processes) {
        return validate(std::span<const Process>(processes));
    }

    /// Trims leading and trailing whitespace characters.
    [[nodiscard]] static std::string_view trim_whitespace(std::string_view str) noexcept;

    /// Normalizes an ID by trimming whitespace and converting to lowercase.
    [[nodiscard]] static std::string normalize_id(std::string_view id);

    /// Checks if a string view consists purely of whitespace characters or is empty.
    [[nodiscard]] static bool is_blank(std::string_view str) noexcept;
};

} // namespace cpu_scheduler
