#include "cpu_scheduler/csv_parser.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

namespace cpu_scheduler {
namespace {

TEST(CsvParserTest, EmptyContentReturnsSuccessWithZeroRows) {
    auto res = CsvParser::parse("");
    EXPECT_TRUE(res.success);
    EXPECT_TRUE(res.rows.empty());
    EXPECT_TRUE(res.errors.empty());
}

TEST(CsvParserTest, StandardUnquotedFields) {
    const std::string csv = "process_id,arrival_time,burst_time\nP1,0,5\nP2,2,3\n";
    auto res = CsvParser::parse(csv);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.rows.size(), 3u);

    EXPECT_EQ(res.rows[0].fields, (std::vector<std::string>{"process_id", "arrival_time", "burst_time"}));
    EXPECT_EQ(res.rows[0].line_number, 1u);

    EXPECT_EQ(res.rows[1].fields, (std::vector<std::string>{"P1", "0", "5"}));
    EXPECT_EQ(res.rows[1].line_number, 2u);

    EXPECT_EQ(res.rows[2].fields, (std::vector<std::string>{"P2", "2", "3"}));
    EXPECT_EQ(res.rows[2].line_number, 3u);
}

TEST(CsvParserTest, QuotedFieldsWithCommas) {
    const std::string csv = "id,name,value\n1,\"Doe, John\",100\n2,\"Smith, Jane\",200\n";
    auto res = CsvParser::parse(csv);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.rows.size(), 3u);
    EXPECT_EQ(res.rows[1].fields[1], "Doe, John");
    EXPECT_EQ(res.rows[2].fields[1], "Smith, Jane");
}

TEST(CsvParserTest, EscapedDoubleQuotesInsideQuotes) {
    const std::string csv = "col1,col2\n\"He said \"\"Hello\"\"\",value\n";
    auto res = CsvParser::parse(csv);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.rows.size(), 2u);
    EXPECT_EQ(res.rows[1].fields[0], "He said \"Hello\"");
    EXPECT_EQ(res.rows[1].fields[1], "value");
}

TEST(CsvParserTest, EmptyFieldsHandledProperly) {
    const std::string csv = "a,,b,c\n,x,,\n";
    auto res = CsvParser::parse(csv);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.rows.size(), 2u);
    EXPECT_EQ(res.rows[0].fields, (std::vector<std::string>{"a", "", "b", "c"}));
    EXPECT_EQ(res.rows[1].fields, (std::vector<std::string>{"", "x", "", ""}));
}

TEST(CsvParserTest, WindowsCrlfLineEndings) {
    const std::string csv = "h1,h2\r\nv1,v2\r\nv3,v4\r\n";
    auto res = CsvParser::parse(csv);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.rows.size(), 3u);
    EXPECT_EQ(res.rows[1].fields[0], "v1");
    EXPECT_EQ(res.rows[1].fields[1], "v2");
    EXPECT_EQ(res.rows[2].fields[0], "v3");
    EXPECT_EQ(res.rows[2].fields[1], "v4");
}

TEST(CsvParserTest, FinalRecordWithoutTrailingNewline) {
    const std::string csv = "a,b\n1,2";
    auto res = CsvParser::parse(csv);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.rows.size(), 2u);
    EXPECT_EQ(res.rows[1].fields, (std::vector<std::string>{"1", "2"}));
}

TEST(CsvParserTest, UnclosedQuoteReportsSyntaxError) {
    const std::string csv = "a,b\n\"unclosed,123\n";
    auto res = CsvParser::parse(csv);

    EXPECT_FALSE(res.success);
    EXPECT_FALSE(res.errors.empty());
    EXPECT_NE(res.errors[0].message.find("Unclosed quote"), std::string::npos);
}

TEST(CsvParserTest, CharactersAfterClosingQuoteBeforeDelimiterReportsError) {
    const std::string csv = "a,b\n\"val\"xyz,123\n";
    auto res = CsvParser::parse(csv);

    EXPECT_FALSE(res.success);
    EXPECT_FALSE(res.errors.empty());
    EXPECT_NE(res.errors[0].message.find("Unexpected character"), std::string::npos);
}

TEST(CsvParserTest, Utf8SpecialCharactersSupported) {
    const std::string csv = "pid,note\nP_α,こんにちは\nP_β,café\n";
    auto res = CsvParser::parse(csv);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.rows.size(), 3u);
    EXPECT_EQ(res.rows[1].fields[0], "P_α");
    EXPECT_EQ(res.rows[1].fields[1], "こんにちは");
    EXPECT_EQ(res.rows[2].fields[0], "P_β");
    EXPECT_EQ(res.rows[2].fields[1], "café");
}

TEST(CsvParserTest, ParseFromStreamMatchesString) {
    const std::string csv = "col1,col2\n10,20\n30,40\n";
    std::istringstream stream(csv);
    auto res = CsvParser::parse_stream(stream);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.rows.size(), 3u);
    EXPECT_EQ(res.rows[1].fields[0], "10");
    EXPECT_EQ(res.rows[2].fields[1], "40");
}

TEST(CsvParserTest, NonExistentFileReportsError) {
    auto res = CsvParser::parse_file("non_existent_file_path_12345.csv");
    EXPECT_FALSE(res.success);
    EXPECT_FALSE(res.errors.empty());
    EXPECT_NE(res.errors[0].message.find("Unable to open file"), std::string::npos);
}

} // namespace
} // namespace cpu_scheduler
