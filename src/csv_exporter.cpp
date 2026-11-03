#include "cpu_scheduler/csv_exporter.hpp"

#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace cpu_scheduler {

namespace {

#ifdef _WIN32

std::string format_win32_error(DWORD error_code) {
    if (error_code == 0) {
        return "Unknown error";
    }
    LPWSTR msg_buf = nullptr;
    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&msg_buf),
        0,
        nullptr);
    std::string result;
    if (size > 0 && msg_buf != nullptr) {
        int utf8_size = WideCharToMultiByte(CP_UTF8, 0, msg_buf, -1, nullptr, 0, nullptr, nullptr);
        if (utf8_size > 0) {
            result.resize(static_cast<std::size_t>(utf8_size - 1));
            WideCharToMultiByte(CP_UTF8, 0, msg_buf, -1, result.data(), utf8_size, nullptr, nullptr);
        }
        LocalFree(msg_buf);
        while (!result.empty() && (result.back() == '\r' || result.back() == '\n' || result.back() == ' ')) {
            result.pop_back();
        }
    }
    if (result.empty()) {
        result = "Error code " + std::to_string(error_code);
    }
    return result;
}

#endif

bool commit_staged_file(
    const std::filesystem::path& temp_path,
    const std::filesystem::path& dest_path,
    bool overwrite,
    std::string& error_message) {
    std::error_code ec;

#ifdef _WIN32
    bool dest_exists = std::filesystem::exists(dest_path, ec);

    if (dest_exists) {
        if (!overwrite) {
            error_message = "Destination file already exists: " + dest_path.string();
            std::filesystem::remove(temp_path, ec);
            return false;
        }

        // Destination exists and overwrite is enabled:
        // Use ReplaceFileW to replace destination in place without deleting it first.
        // If ReplaceFileW fails, the original destination file is preserved untouched.
        if (!ReplaceFileW(
                dest_path.c_str(),
                temp_path.c_str(),
                nullptr,
                0,
                nullptr,
                nullptr)) {
            DWORD err = GetLastError();
            // If destination was deleted between exists check and ReplaceFileW, fallback to MoveFileExW
            if (err == ERROR_FILE_NOT_FOUND) {
                if (MoveFileExW(temp_path.c_str(), dest_path.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING)) {
                    return true;
                }
                err = GetLastError();
            }
            error_message = "Failed to replace destination file '" + dest_path.string() + "': " + format_win32_error(err);
            std::filesystem::remove(temp_path, ec);
            return false;
        }
        return true;
    } else {
        // Destination does not exist:
        // Move temporary file to destination. If overwrite is disabled, do NOT specify
        // MOVEFILE_REPLACE_EXISTING, preventing a race that silently replaces a concurrent file.
        DWORD flags = MOVEFILE_COPY_ALLOWED;
        if (overwrite) {
            flags |= MOVEFILE_REPLACE_EXISTING;
        }
        if (!MoveFileExW(temp_path.c_str(), dest_path.c_str(), flags)) {
            DWORD err = GetLastError();
            if (err == ERROR_ALREADY_EXISTS || err == ERROR_FILE_EXISTS) {
                error_message = "Destination file already exists: " + dest_path.string();
            } else {
                error_message = "Failed to move temporary file to destination '" + dest_path.string() + "': " + format_win32_error(err);
            }
            std::filesystem::remove(temp_path, ec);
            return false;
        }
        return true;
    }
#else
    if (std::filesystem::exists(dest_path, ec)) {
        if (!overwrite) {
            error_message = "Destination file already exists: " + dest_path.string();
            std::filesystem::remove(temp_path, ec);
            return false;
        }
    }
    std::filesystem::rename(temp_path, dest_path, ec);
    if (ec) {
        error_message = "Failed to rename temporary file to destination: " + ec.message();
        std::error_code rm_ec;
        std::filesystem::remove(temp_path, rm_ec);
        return false;
    }
    return true;
#endif
}

} // namespace


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

    if (!commit_staged_file(temp_path, path, overwrite, res.error_message)) {
        res.success = false;
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

    if (!commit_staged_file(temp_path, path, overwrite, res.error_message)) {
        res.success = false;
        return res;
    }

    res.success = true;
    return res;
}

} // namespace cpu_scheduler
