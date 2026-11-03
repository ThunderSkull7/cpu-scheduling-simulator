#pragma once

#include "cpu_scheduler/scheduling_result.hpp"

#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>

namespace cpu_scheduler {

/**
 * @brief Configuration options for CSV result and timeline export.
 */
struct CsvExportOptions {
    bool sanitize_formula_injection{true}; ///< Neutralize spreadsheet formula characters (=, +, -, @)
    bool include_priority{true};           ///< Include priority column in process metrics export
    bool include_queue_id{true};           ///< Include queue_id column in metrics and timeline export
    std::string line_ending{"\n"};         ///< Line ending to use (\n or \r\n)

    constexpr CsvExportOptions() = default;
};

/**
 * @brief Result outcome for file-based CSV export operations.
 */
struct CsvExportResult {
    bool success{false};
    std::string error_message;
    std::filesystem::path output_path;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

/**
 * @brief Exports scheduling results and execution timelines to standardized CSV files.
 *
 * Provides RFC 4180 escaping, spreadsheet formula injection defense for text fields,
 * safe file writing with transactional replacement on Windows, and stream-writing interfaces.
 */
class CsvExporter {
public:
    CsvExporter() = delete;

    /**
     * @brief Neutralizes potential spreadsheet formula injection in a text string.
     *
     * If the string begins with '=', '+', '-', or '@', prepends a single quote (').
     * Does NOT quote or escape; call escape_field() on the result.
     */
    [[nodiscard]] static std::string sanitize_formula(std::string_view text);

    /**
     * @brief Quotes and escapes a CSV field value according to RFC 4180 rules.
     */
    [[nodiscard]] static std::string escape_field(std::string_view text, bool sanitize = false);

    /**
     * @brief Exports process execution metrics to a CSV string.
     */
    [[nodiscard]] static std::string export_process_metrics_to_string(
        const SchedulingResult& result,
        const CsvExportOptions& options = {});

    /**
     * @brief Exports process execution metrics to an output stream.
     */
    static bool export_process_metrics_to_stream(
        std::ostream& out,
        const SchedulingResult& result,
        const CsvExportOptions& options = {});

    /**
     * @brief Exports process execution metrics to a file on disk.
     *
     * Staged write to a temporary file with safe in-place replacement (via ReplaceFileW on Windows).
     * The original destination file is preserved untouched if replacement fails.
     */
    [[nodiscard]] static CsvExportResult export_process_metrics_to_file(
        const std::filesystem::path& path,
        const SchedulingResult& result,
        bool overwrite = true,
        const CsvExportOptions& options = {});

    /**
     * @brief Exports the chronological execution timeline to a CSV string.
     */
    [[nodiscard]] static std::string export_timeline_to_string(
        const SchedulingResult& result,
        const CsvExportOptions& options = {});

    /**
     * @brief Exports the chronological execution timeline to an output stream.
     */
    static bool export_timeline_to_stream(
        std::ostream& out,
        const SchedulingResult& result,
        const CsvExportOptions& options = {});

    /**
     * @brief Exports the chronological execution timeline to a file on disk.
     *
     * Staged write to a temporary file with safe in-place replacement (via ReplaceFileW on Windows).
     * The original destination file is preserved untouched if replacement fails.
     */
    [[nodiscard]] static CsvExportResult export_timeline_to_file(
        const std::filesystem::path& path,
        const SchedulingResult& result,
        bool overwrite = true,
        const CsvExportOptions& options = {});
};

} // namespace cpu_scheduler
