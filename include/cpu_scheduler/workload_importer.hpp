#pragma once

#include "cpu_scheduler/process.hpp"

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cpu_scheduler {

/**
 * @brief An individual error diagnostic reported during CSV workload import.
 */
struct WorkloadImportError {
    std::size_t row{0};       ///< 1-based source CSV line number (0 if header/global error)
    std::string column;       ///< Column name involved in the error (if applicable)
    std::string message;      ///< Human-readable diagnostic description

    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] bool operator==(const WorkloadImportError& other) const = default;
};

/**
 * @brief Represents the outcome of importing a workload from CSV.
 */
struct WorkloadImportResult {
    bool success{false};
    std::vector<Process> processes;
    std::vector<std::string> queue_bindings; ///< Parallel to processes if queue_id was present
    bool has_priority{false};                ///< Whether priority column was present in CSV
    bool has_queue_id{false};                ///< Whether queue_id column was present in CSV
    std::vector<WorkloadImportError> errors;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

/**
 * @brief Ingests and validates process workloads from CSV formats.
 *
 * Implements strict, all-or-nothing validation, duplicate process ID rejection,
 * numeric bounds and overflow protection, header aliasing, and comprehensive row diagnostics.
 */
class WorkloadImporter {
public:
    static constexpr std::size_t DefaultMaxRecords = 100;

    WorkloadImporter() = delete;

    /**
     * @brief Imports a process workload from an in-memory CSV string.
     */
    [[nodiscard]] static WorkloadImportResult import_from_string(
        std::string_view csv_content,
        std::size_t max_records = DefaultMaxRecords);

    /**
     * @brief Imports a process workload from an input stream.
     */
    [[nodiscard]] static WorkloadImportResult import_from_stream(
        std::istream& in,
        std::size_t max_records = DefaultMaxRecords);

    /**
     * @brief Imports a process workload from a file on disk.
     */
    [[nodiscard]] static WorkloadImportResult import_from_file(
        const std::filesystem::path& file_path,
        std::size_t max_records = DefaultMaxRecords);
};

} // namespace cpu_scheduler
