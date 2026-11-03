#include "cpu_scheduler/csv_exporter.hpp"

#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_map>

namespace cpu_scheduler {

std::string CsvExporter::sanitize_formula(std::string_view text) {
    if (text.empty()) {
        return "";
    }
    char first = text.front();
    if (first == '=' || first == '+' || first == '-' || first == '@' || first == '\t' || first == '\r') {
        return "'" + std::string(text);
    }
    return std::string(text);
}

std::string CsvExporter::escape_field(std::string_view text, bool sanitize) {
    std::string val = sanitize ? sanitize_formula(text) : std::string(text);
    bool needs_quotes = false;
    for (char c : val) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needs_quotes = true;
            break;
        }
    }
    if (!val.empty() && val.front() == '\'') {
        needs_quotes = true;
    }

    if (!needs_quotes) {
        return val;
    }

    std::string escaped = "\"";
    for (char c : val) {
        if (c == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(c);
        }
    }
    escaped += "\"";
    return escaped;
}

std::string CsvExporter::export_process_metrics_to_string(
    const SchedulingResult& result,
    const CsvExportOptions& options) {
    std::ostringstream ss;
    export_process_metrics_to_stream(ss, result, options);
    return ss.str();
}

bool CsvExporter::export_process_metrics_to_stream(
    std::ostream& out,
    const SchedulingResult& result,
    const CsvExportOptions& options) {
    const std::string& le = options.line_ending;

    // Header
    out << "process_id,arrival_time,burst_time,completion_time,turnaround_time,waiting_time,response_time";
    if (options.include_priority) {
        out << ",priority";
    }
    if (options.include_queue_id) {
        out << ",queue_id";
    }
    out << le;

    // Rows
    for (const auto& proc : result.processes()) {
        out << escape_field(proc.process_id, options.sanitize_formula_injection) << ","
            << proc.arrival_time << ","
            << proc.burst_time << ","
            << proc.completion_time << ","
            << proc.turnaround_time << ","
            << proc.waiting_time << ","
            << proc.response_time;

        if (options.include_priority) {
            out << "," << proc.priority;
        }
        if (options.include_queue_id) {
            out << "," << escape_field(proc.queue_id, options.sanitize_formula_injection);
        }
        out << le;
    }

    return out.good();
}

CsvExportResult CsvExporter::export_process_metrics_to_file(
    const std::filesystem::path& path,
    const SchedulingResult& result,
    bool overwrite,
    const CsvExportOptions& options) {
    CsvExportResult res;
    res.output_path = path;

    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        if (!overwrite) {
            res.success = false;
            res.error_message = "Destination file already exists: " + path.string();
            return res;
        }
    }

    auto parent = path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent, ec)) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            res.success = false;
            res.error_message = "Failed to create directory '" + parent.string() + "': " + ec.message();
            return res;
        }
    }

    std::filesystem::path temp_path = path;
    temp_path += ".tmp";

    {
        std::ofstream file(temp_path, std::ios::binary);
        if (!file.is_open()) {
            res.success = false;
            res.error_message = "Unable to create output file: " + temp_path.string();
            return res;
        }

        if (!export_process_metrics_to_stream(file, result, options)) {
            file.close();
            std::filesystem::remove(temp_path, ec);
            res.success = false;
            res.error_message = "Failed during CSV stream writing.";
            return res;
        }
    }

    if (std::filesystem::exists(path, ec)) {
        std::filesystem::remove(path, ec);
    }
    std::filesystem::rename(temp_path, path, ec);
    if (ec) {
        std::filesystem::remove(temp_path, ec);
        res.success = false;
        res.error_message = "Failed to rename temporary file to destination: " + ec.message();
        return res;
    }

    res.success = true;
    return res;
}

std::string CsvExporter::export_timeline_to_string(
    const SchedulingResult& result,
    const CsvExportOptions& options) {
    std::ostringstream ss;
    export_timeline_to_stream(ss, result, options);
    return ss.str();
}

bool CsvExporter::export_timeline_to_stream(
    std::ostream& out,
    const SchedulingResult& result,
    const CsvExportOptions& options) {
    const std::string& le = options.line_ending;

    out << "start_time,end_time,process_id";
    if (options.include_queue_id) {
        out << ",queue_id";
    }
    out << le;

    std::unordered_map<std::string, std::string> queue_map;
    for (const auto& proc : result.processes()) {
        if (!proc.queue_id.empty()) {
            queue_map[proc.process_id] = proc.queue_id;
        }
    }

    for (const auto& seg : result.timeline()) {
        out << seg.start_time << "," << seg.end_time << ",";
        if (seg.is_idle()) {
            out << "IDLE";
            if (options.include_queue_id) {
                out << ",";
            }
        } else {
            std::string pid = seg.process_id.value_or("?");
            out << escape_field(pid, options.sanitize_formula_injection);
            if (options.include_queue_id) {
                auto it = queue_map.find(pid);
                if (it != queue_map.end()) {
                    out << "," << escape_field(it->second, options.sanitize_formula_injection);
                } else {
                    out << ",";
                }
            }
        }
        out << le;
    }

    return out.good();
}

CsvExportResult CsvExporter::export_timeline_to_file(
    const std::filesystem::path& path,
    const SchedulingResult& result,
    bool overwrite,
    const CsvExportOptions& options) {
    CsvExportResult res;
    res.output_path = path;

    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        if (!overwrite) {
            res.success = false;
            res.error_message = "Destination file already exists: " + path.string();
            return res;
        }
    }

    auto parent = path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent, ec)) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            res.success = false;
            res.error_message = "Failed to create directory '" + parent.string() + "': " + ec.message();
            return res;
        }
    }

    std::filesystem::path temp_path = path;
    temp_path += ".tmp";

    {
        std::ofstream file(temp_path, std::ios::binary);
        if (!file.is_open()) {
            res.success = false;
            res.error_message = "Unable to create output file: " + temp_path.string();
            return res;
        }

        if (!export_timeline_to_stream(file, result, options)) {
            file.close();
            std::filesystem::remove(temp_path, ec);
            res.success = false;
            res.error_message = "Failed during timeline CSV writing.";
            return res;
        }
    }

    if (std::filesystem::exists(path, ec)) {
        std::filesystem::remove(path, ec);
    }
    std::filesystem::rename(temp_path, path, ec);
    if (ec) {
        std::filesystem::remove(temp_path, ec);
        res.success = false;
        res.error_message = "Failed to rename temporary file to destination: " + ec.message();
        return res;
    }

    res.success = true;
    return res;
}

} // namespace cpu_scheduler
