#include "cpu_scheduler/workload_importer.hpp"
#include "cpu_scheduler/csv_parser.hpp"
#include "cpu_scheduler/process_validator.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace cpu_scheduler {

namespace {

std::string trim(std::string_view sv) {
    auto start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return "";
    }
    auto end = sv.find_last_not_of(" \t\r\n");
    return std::string(sv.substr(start, end - start + 1));
}

std::string to_lower(std::string_view sv) {
    std::string s;
    s.reserve(sv.size());
    for (char c : sv) {
        s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return s;
}

enum class ColumnRole {
    ProcessId,
    ArrivalTime,
    BurstTime,
    Priority,
    QueueId,
    Unknown
};

ColumnRole identify_column(std::string_view header) {
    std::string h = to_lower(trim(header));
    if (h == "process_id" || h == "pid" || h == "process" || h == "id") {
        return ColumnRole::ProcessId;
    }
    if (h == "arrival_time" || h == "arrival" || h == "at") {
        return ColumnRole::ArrivalTime;
    }
    if (h == "burst_time" || h == "burst" || h == "bt") {
        return ColumnRole::BurstTime;
    }
    if (h == "priority" || h == "prio") {
        return ColumnRole::Priority;
    }
    if (h == "queue_id" || h == "queue" || h == "qid") {
        return ColumnRole::QueueId;
    }
    return ColumnRole::Unknown;
}

bool parse_int64(std::string_view text, std::int64_t& out_val) {
    std::string t = trim(text);
    if (t.empty()) {
        return false;
    }

    const char* start = t.data();
    const char* end = t.data() + t.size();
    auto [ptr, ec] = std::from_chars(start, end, out_val);
    return (ec == std::errc{} && ptr == end);
}

bool parse_int(std::string_view text, int& out_val) {
    std::string t = trim(text);
    if (t.empty()) {
        return false;
    }

    const char* start = t.data();
    const char* end = t.data() + t.size();
    auto [ptr, ec] = std::from_chars(start, end, out_val);
    return (ec == std::errc{} && ptr == end);
}

} // namespace

std::string WorkloadImportError::to_string() const {
    std::ostringstream ss;
    if (row > 0) {
        ss << "Row " << row << ": ";
    }
    if (!column.empty()) {
        ss << "'" << column << "' - ";
    }
    ss << message;
    return ss.str();
}

WorkloadImportResult WorkloadImporter::import_from_string(
    std::string_view csv_content,
    std::size_t max_records) {
    WorkloadImportResult result;

    CsvParseResult parse_res = CsvParser::parse(csv_content);
    if (!parse_res.success) {
        for (const auto& err : parse_res.errors) {
            result.errors.push_back(WorkloadImportError{
                .row = err.line,
                .column = "",
                .message = "CSV syntax error: " + err.message
            });
        }
        result.success = false;
        return result;
    }

    if (parse_res.rows.empty()) {
        result.errors.push_back(WorkloadImportError{
            .row = 0,
            .column = "",
            .message = "CSV content is empty; at least a header row is required."
        });
        result.success = false;
        return result;
    }

    // 1. Validate Header Row
    const auto& header_row = parse_res.rows[0];
    if (header_row.empty()) {
        result.errors.push_back(WorkloadImportError{
            .row = header_row.line_number,
            .column = "",
            .message = "Header row contains no columns."
        });
        result.success = false;
        return result;
    }

    std::optional<std::size_t> pid_col;
    std::optional<std::size_t> arr_col;
    std::optional<std::size_t> burst_col;
    std::optional<std::size_t> prio_col;
    std::optional<std::size_t> queue_col;

    std::unordered_set<std::string> seen_headers;

    for (std::size_t c = 0; c < header_row.size(); ++c) {
        std::string raw_header = trim(header_row[c]);
        std::string lower_header = to_lower(raw_header);

        if (seen_headers.contains(lower_header)) {
            result.errors.push_back(WorkloadImportError{
                .row = header_row.line_number,
                .column = raw_header,
                .message = "Duplicate column header '" + raw_header + "'."
            });
            continue;
        }
        seen_headers.insert(lower_header);

        ColumnRole role = identify_column(raw_header);
        switch (role) {
            case ColumnRole::ProcessId:
                if (pid_col.has_value()) {
                    result.errors.push_back(WorkloadImportError{
                        .row = header_row.line_number,
                        .column = raw_header,
                        .message = "Ambiguous header: multiple columns resolve to 'process_id'."
                    });
                } else {
                    pid_col = c;
                }
                break;
            case ColumnRole::ArrivalTime:
                if (arr_col.has_value()) {
                    result.errors.push_back(WorkloadImportError{
                        .row = header_row.line_number,
                        .column = raw_header,
                        .message = "Ambiguous header: multiple columns resolve to 'arrival_time'."
                    });
                } else {
                    arr_col = c;
                }
                break;
            case ColumnRole::BurstTime:
                if (burst_col.has_value()) {
                    result.errors.push_back(WorkloadImportError{
                        .row = header_row.line_number,
                        .column = raw_header,
                        .message = "Ambiguous header: multiple columns resolve to 'burst_time'."
                    });
                } else {
                    burst_col = c;
                }
                break;
            case ColumnRole::Priority:
                if (prio_col.has_value()) {
                    result.errors.push_back(WorkloadImportError{
                        .row = header_row.line_number,
                        .column = raw_header,
                        .message = "Ambiguous header: multiple columns resolve to 'priority'."
                    });
                } else {
                    prio_col = c;
                }
                break;
            case ColumnRole::QueueId:
                if (queue_col.has_value()) {
                    result.errors.push_back(WorkloadImportError{
                        .row = header_row.line_number,
                        .column = raw_header,
                        .message = "Ambiguous header: multiple columns resolve to 'queue_id'."
                    });
                } else {
                    queue_col = c;
                }
                break;
            case ColumnRole::Unknown:
                result.errors.push_back(WorkloadImportError{
                    .row = header_row.line_number,
                    .column = raw_header,
                    .message = "Unsupported column header '" + raw_header + "'."
                });
                break;
        }
    }

    if (!pid_col.has_value()) {
        result.errors.push_back(WorkloadImportError{
            .row = header_row.line_number,
            .column = "process_id",
            .message = "Missing required column 'process_id'."
        });
    }
    if (!arr_col.has_value()) {
        result.errors.push_back(WorkloadImportError{
            .row = header_row.line_number,
            .column = "arrival_time",
            .message = "Missing required column 'arrival_time'."
        });
    }
    if (!burst_col.has_value()) {
        result.errors.push_back(WorkloadImportError{
            .row = header_row.line_number,
            .column = "burst_time",
            .message = "Missing required column 'burst_time'."
        });
    }

    if (!result.errors.empty()) {
        result.success = false;
        return result;
    }

    result.has_priority = prio_col.has_value();
    result.has_queue_id = queue_col.has_value();

    // 2. Validate Data Rows
    std::size_t data_row_count = parse_res.rows.size() - 1;
    if (data_row_count == 0) {
        result.errors.push_back(WorkloadImportError{
            .row = header_row.line_number,
            .column = "",
            .message = "CSV contains a header but no process data rows."
        });
        result.success = false;
        return result;
    }

    if (data_row_count > max_records) {
        result.errors.push_back(WorkloadImportError{
            .row = 0,
            .column = "",
            .message = "Workload record count (" + std::to_string(data_row_count) +
                       ") exceeds maximum allowed limit of " + std::to_string(max_records) + "."
        });
        result.success = false;
        return result;
    }

    std::unordered_map<std::string, std::size_t> seen_pids; // normalized ID -> original row

    for (std::size_t r = 1; r < parse_res.rows.size(); ++r) {
        const auto& row = parse_res.rows[r];

        if (row.size() != header_row.size()) {
            result.errors.push_back(WorkloadImportError{
                .row = row.line_number,
                .column = "",
                .message = "Field count mismatch: expected " + std::to_string(header_row.size()) +
                           " fields, got " + std::to_string(row.size()) + "."
            });
            continue;
        }

        // Process ID
        std::string raw_pid = trim(row[*pid_col]);
        if (raw_pid.empty()) {
            result.errors.push_back(WorkloadImportError{
                .row = row.line_number,
                .column = header_row[*pid_col],
                .message = "Process ID cannot be empty or whitespace-only."
            });
        } else {
            std::string norm_pid = to_lower(raw_pid);
            auto it = seen_pids.find(norm_pid);
            if (it != seen_pids.end()) {
                result.errors.push_back(WorkloadImportError{
                    .row = row.line_number,
                    .column = header_row[*pid_col],
                    .message = "Duplicate process ID '" + raw_pid +
                               "' (already defined at row " + std::to_string(it->second) + ")."
                });
            } else {
                seen_pids[norm_pid] = row.line_number;
            }
        }

        // Arrival Time
        std::int64_t arr_val = 0;
        if (!parse_int64(row[*arr_col], arr_val)) {
            result.errors.push_back(WorkloadImportError{
                .row = row.line_number,
                .column = header_row[*arr_col],
                .message = "Arrival time must be a valid integer, got '" + row[*arr_col] + "'."
            });
        } else if (arr_val < 0) {
            result.errors.push_back(WorkloadImportError{
                .row = row.line_number,
                .column = header_row[*arr_col],
                .message = "Arrival time cannot be negative, got '" + std::to_string(arr_val) + "'."
            });
        }

        // Burst Time
        std::int64_t burst_val = 0;
        if (!parse_int64(row[*burst_col], burst_val)) {
            result.errors.push_back(WorkloadImportError{
                .row = row.line_number,
                .column = header_row[*burst_col],
                .message = "Burst time must be a valid integer, got '" + row[*burst_col] + "'."
            });
        } else if (burst_val <= 0) {
            result.errors.push_back(WorkloadImportError{
                .row = row.line_number,
                .column = header_row[*burst_col],
                .message = "Burst time must be strictly positive (> 0), got '" + std::to_string(burst_val) + "'."
            });
        }

        // Priority (optional)
        int prio_val = 0;
        if (prio_col.has_value()) {
            std::string p_str = trim(row[*prio_col]);
            if (!p_str.empty()) {
                if (!parse_int(p_str, prio_val)) {
                    result.errors.push_back(WorkloadImportError{
                        .row = row.line_number,
                        .column = header_row[*prio_col],
                        .message = "Priority must be a valid integer, got '" + p_str + "'."
                    });
                }
            }
        }

        // Queue ID (optional)
        std::string q_val;
        if (queue_col.has_value()) {
            q_val = trim(row[*queue_col]);
        }

        if (result.errors.empty()) {
            result.processes.push_back(Process(raw_pid, arr_val, burst_val, prio_val));
            if (queue_col.has_value()) {
                result.queue_bindings.push_back(std::move(q_val));
            }
        }
    }

    if (!result.errors.empty()) {
        result.processes.clear();
        result.queue_bindings.clear();
        result.success = false;
    } else {
        result.success = true;
    }

    return result;
}

WorkloadImportResult WorkloadImporter::import_from_stream(
    std::istream& in,
    std::size_t max_records) {
    std::ostringstream ss;
    ss << in.rdbuf();
    return import_from_string(ss.str(), max_records);
}

WorkloadImportResult WorkloadImporter::import_from_file(
    const std::filesystem::path& file_path,
    std::size_t max_records) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        WorkloadImportResult res;
        res.success = false;
        res.errors.push_back(WorkloadImportError{
            .row = 0,
            .column = "",
            .message = "Unable to open file: " + file_path.string()
        });
        return res;
    }
    return import_from_stream(file, max_records);
}

} // namespace cpu_scheduler
