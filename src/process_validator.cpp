#include "cpu_scheduler/process_validator.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>

namespace cpu_scheduler {

std::string ValidationResult::summary() const {
    if (is_valid()) {
        return "Validation passed: workload is valid.";
    }

    std::ostringstream oss;
    oss << "Validation failed with " << errors_.size() << " error(s):\n";
    for (std::size_t i = 0; i < errors_.size(); ++i) {
        oss << "  [" << (i + 1) << "] " << errors_[i].message << "\n";
    }
    return oss.str();
}

std::string_view ProcessValidator::trim_whitespace(std::string_view str) noexcept {
    const auto first = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });

    if (first == str.end()) {
        return {};
    }

    const auto last = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();

    return str.substr(static_cast<std::size_t>(first - str.begin()),
                      static_cast<std::size_t>(last - first));
}

bool ProcessValidator::is_blank(std::string_view str) noexcept {
    return trim_whitespace(str).empty();
}

std::string ProcessValidator::normalize_id(std::string_view id) {
    const auto trimmed = trim_whitespace(id);
    std::string normalized;
    normalized.reserve(trimmed.size());

    for (const char ch : trimmed) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    return normalized;
}

ValidationResult ProcessValidator::validate(std::span<const Process> processes) {
    std::vector<ValidationError> errors;

    if (processes.empty()) {
        errors.push_back(ValidationError{
            .code = ValidationErrorCode::EmptyWorkload,
            .message = "Workload is empty. At least one process must be supplied.",
            .process_id = std::nullopt,
            .process_index = std::nullopt
        });
        return ValidationResult(std::move(errors));
    }

    struct SeenProcessInfo {
        std::string original_id;
        std::size_t index;
    };

    std::unordered_map<std::string, SeenProcessInfo> seen_ids;

    for (std::size_t i = 0; i < processes.size(); ++i) {
        const auto& proc = processes[i];

        // 1. Validate Process ID
        if (is_blank(proc.id)) {
            errors.push_back(ValidationError{
                .code = ValidationErrorCode::EmptyProcessId,
                .message = "Process at index " + std::to_string(i) +
                           " has an empty or whitespace-only ID.",
                .process_id = proc.id,
                .process_index = i
            });
        } else {
            const std::string normalized = normalize_id(proc.id);
            if (const auto it = seen_ids.find(normalized); it != seen_ids.end()) {
                errors.push_back(ValidationError{
                    .code = ValidationErrorCode::DuplicateProcessId,
                    .message = "Duplicate process ID '" + proc.id + "' at index " +
                               std::to_string(i) + " conflicts with process '" +
                               it->second.original_id + "' at index " +
                               std::to_string(it->second.index) +
                               " (process IDs are case-insensitive and ignore surrounding whitespace).",
                    .process_id = proc.id,
                    .process_index = i
                });
            } else {
                seen_ids.emplace(normalized, SeenProcessInfo{proc.id, i});
            }
        }

        // 2. Validate Arrival Time (must be >= 0)
        if (proc.arrival_time < 0) {
            errors.push_back(ValidationError{
                .code = ValidationErrorCode::NegativeArrivalTime,
                .message = "Process '" + proc.id + "' at index " + std::to_string(i) +
                           " has negative arrival time: " + std::to_string(proc.arrival_time) +
                           ". Arrival times must be non-negative (>= 0).",
                .process_id = proc.id,
                .process_index = i
            });
        }

        // 3. Validate Burst Time (must be > 0)
        if (proc.burst_time <= 0) {
            errors.push_back(ValidationError{
                .code = ValidationErrorCode::NonPositiveBurstTime,
                .message = "Process '" + proc.id + "' at index " + std::to_string(i) +
                           " has non-positive burst time: " + std::to_string(proc.burst_time) +
                           ". Burst times must be strictly positive (> 0).",
                .process_id = proc.id,
                .process_index = i
            });
        }
    }

    return ValidationResult(std::move(errors));
}

} // namespace cpu_scheduler
