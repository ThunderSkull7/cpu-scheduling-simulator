#include "cpu_scheduler/csv_parser.hpp"

#include <fstream>
#include <sstream>

namespace cpu_scheduler {

namespace {

enum class ParseState {
    FieldStart,
    UnquotedField,
    QuotedField,
    AfterClosingQuote
};

} // namespace

CsvParseResult CsvParser::parse(std::string_view content) {
    CsvParseResult result;
    result.success = true;

    if (content.empty()) {
        return result;
    }

    std::vector<CsvRow> rows;
    std::vector<std::string> current_row_fields;
    std::string current_field;

    std::size_t line = 1;
    std::size_t col = 1;
    std::size_t row_start_line = 1;
    std::size_t quote_start_line = 1;
    std::size_t quote_start_col = 1;

    ParseState state = ParseState::FieldStart;
    bool row_has_data = false;

    auto finish_field = [&]() {
        current_row_fields.push_back(std::move(current_field));
        current_field.clear();
        state = ParseState::FieldStart;
        row_has_data = true;
    };

    auto finish_row = [&]() {
        if (row_has_data || !current_row_fields.empty()) {
            rows.push_back(CsvRow{
                .fields = std::move(current_row_fields),
                .line_number = row_start_line
            });
            current_row_fields.clear();
            row_has_data = false;
        }
        row_start_line = line;
    };

    std::size_t i = 0;
    const std::size_t n = content.size();

    while (i < n) {
        char ch = content[i];

        switch (state) {
            case ParseState::FieldStart: {
                if (current_row_fields.empty() && !row_has_data) {
                    row_start_line = line;
                }

                if (ch == '"') {
                    state = ParseState::QuotedField;
                    quote_start_line = line;
                    quote_start_col = col;
                    row_has_data = true;
                    ++i;
                    ++col;
                } else if (ch == ',') {
                    finish_field();
                    ++i;
                    ++col;
                } else if (ch == '\r' || ch == '\n') {
                    // Check for empty line
                    if (row_has_data || !current_row_fields.empty()) {
                        finish_field();
                        finish_row();
                    }
                    if (ch == '\r' && i + 1 < n && content[i + 1] == '\n') {
                        ++i;
                    }
                    ++i;
                    ++line;
                    col = 1;
                    row_start_line = line;
                } else {
                    current_field.push_back(ch);
                    state = ParseState::UnquotedField;
                    row_has_data = true;
                    ++i;
                    ++col;
                }
                break;
            }

            case ParseState::UnquotedField: {
                if (ch == ',') {
                    finish_field();
                    ++i;
                    ++col;
                } else if (ch == '\r' || ch == '\n') {
                    finish_field();
                    finish_row();
                    if (ch == '\r' && i + 1 < n && content[i + 1] == '\n') {
                        ++i;
                    }
                    ++i;
                    ++line;
                    col = 1;
                    row_start_line = line;
                } else if (ch == '"') {
                    result.errors.push_back(CsvParseError{
                        .line = line,
                        .column = col,
                        .message = "Unexpected quote in unquoted field."
                    });
                    result.success = false;
                    current_field.push_back(ch);
                    ++i;
                    ++col;
                } else {
                    current_field.push_back(ch);
                    ++i;
                    ++col;
                }
                break;
            }

            case ParseState::QuotedField: {
                if (ch == '"') {
                    if (i + 1 < n && content[i + 1] == '"') {
                        // Escaped quote: "" -> "
                        current_field.push_back('"');
                        i += 2;
                        col += 2;
                    } else {
                        // Closing quote
                        state = ParseState::AfterClosingQuote;
                        ++i;
                        ++col;
                    }
                } else if (ch == '\r' || ch == '\n') {
                    current_field.push_back(ch);
                    if (ch == '\r' && i + 1 < n && content[i + 1] == '\n') {
                        current_field.push_back(content[i + 1]);
                        ++i;
                    }
                    ++i;
                    ++line;
                    col = 1;
                } else {
                    current_field.push_back(ch);
                    ++i;
                    ++col;
                }
                break;
            }

            case ParseState::AfterClosingQuote: {
                if (ch == ',') {
                    finish_field();
                    ++i;
                    ++col;
                } else if (ch == '\r' || ch == '\n') {
                    finish_field();
                    finish_row();
                    if (ch == '\r' && i + 1 < n && content[i + 1] == '\n') {
                        ++i;
                    }
                    ++i;
                    ++line;
                    col = 1;
                    row_start_line = line;
                } else if (ch == ' ' || ch == '\t') {
                    // Leniently skip trailing whitespace after closing quote
                    ++i;
                    ++col;
                } else {
                    result.errors.push_back(CsvParseError{
                        .line = line,
                        .column = col,
                        .message = std::string("Unexpected character '") + ch +
                                   "' after closing quote before delimiter."
                    });
                    result.success = false;
                    // Treat as part of field and continue
                    current_field.push_back(ch);
                    state = ParseState::UnquotedField;
                    ++i;
                    ++col;
                }
                break;
            }
        }
    }

    // Handle end of file
    if (state == ParseState::QuotedField) {
        result.errors.push_back(CsvParseError{
            .line = quote_start_line,
            .column = quote_start_col,
            .message = "Unclosed quote at end of input."
        });
        result.success = false;
    } else if (state == ParseState::UnquotedField || state == ParseState::AfterClosingQuote || row_has_data) {
        finish_field();
        finish_row();
    }

    if (result.errors.empty()) {
        result.rows = std::move(rows);
        result.success = true;
    } else {
        result.success = false;
    }

    return result;
}

CsvParseResult CsvParser::parse_stream(std::istream& in) {
    std::ostringstream ss;
    ss << in.rdbuf();
    return parse(ss.str());
}

CsvParseResult CsvParser::parse_file(const std::filesystem::path& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        CsvParseResult res;
        res.success = false;
        res.errors.push_back(CsvParseError{
            .line = 0,
            .column = 0,
            .message = "Unable to open file: " + file_path.string()
        });
        return res;
    }
    return parse_stream(file);
}

} // namespace cpu_scheduler
