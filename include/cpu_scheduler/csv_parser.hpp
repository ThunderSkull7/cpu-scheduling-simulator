#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace cpu_scheduler {

/**
 * @brief Represents a single parsed row from a CSV document.
 */
struct CsvRow {
    std::vector<std::string> fields;
    std::size_t line_number{0}; ///< 1-based original source line number

    [[nodiscard]] std::size_t size() const noexcept {
        return fields.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return fields.empty();
    }

    [[nodiscard]] const std::string& operator[](std::size_t index) const {
        return fields[index];
    }

    [[nodiscard]] const std::string& at(std::size_t index) const {
        return fields.at(index);
    }

    [[nodiscard]] bool operator==(const CsvRow& other) const = default;
};

/**
 * @brief Represents a structural syntax error encountered during CSV parsing.
 */
struct CsvParseError {
    std::size_t line{0};
    std::size_t column{0};
    std::string message;

    [[nodiscard]] bool operator==(const CsvParseError& other) const = default;
};

/**
 * @brief Outcome of a CSV parsing operation.
 */
struct CsvParseResult {
    bool success{false};
    std::vector<CsvRow> rows;
    std::vector<CsvParseError> errors;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

/**
 * @brief Robust, RFC 4180-compliant CSV parser.
 *
 * Supports comma delimiters, quoted fields, embedded commas, escaped double quotes (""),
 * newlines within quotes, both CRLF and LF line endings, empty fields, files without trailing
 * newlines, and detection of malformed quoting syntax.
 */
class CsvParser {
public:
    CsvParser() = delete;

    /**
     * @brief Parses CSV content from an in-memory string view.
     */
    [[nodiscard]] static CsvParseResult parse(std::string_view content);

    /**
     * @brief Parses CSV content from an input stream.
     */
    [[nodiscard]] static CsvParseResult parse_stream(std::istream& in);

    /**
     * @brief Parses CSV content from a file on disk.
     */
    [[nodiscard]] static CsvParseResult parse_file(const std::filesystem::path& file_path);
};

} // namespace cpu_scheduler
