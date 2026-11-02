#include "cpu_scheduler/process_validator.hpp"

#include <gtest/gtest.h>
#include <algorithm>
#include <vector>

namespace cpu_scheduler::testing {

namespace {

bool has_error_code(const ValidationResult& result, ValidationErrorCode expected_code) {
    return std::any_of(result.errors().begin(), result.errors().end(),
                       [expected_code](const ValidationError& err) {
                           return err.code == expected_code;
                       });
}

std::size_t count_error_code(const ValidationResult& result, ValidationErrorCode expected_code) {
    return static_cast<std::size_t>(
        std::count_if(result.errors().begin(), result.errors().end(),
                      [expected_code](const ValidationError& err) {
                          return err.code == expected_code;
                      }));
}

} // namespace

// 1. Valid workload
TEST(ProcessValidatorTest, ValidWorkload) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5, 1),
        Process("P2", 2, 3, 2),
        Process("P3", 4, 8, 3)
    };

    const auto result = ProcessValidator::validate(workload);

    EXPECT_TRUE(result.is_valid());
    EXPECT_TRUE(result);
    EXPECT_EQ(result.error_count(), 0);
    EXPECT_TRUE(result.errors().empty());
}

// 2. Empty workload
TEST(ProcessValidatorTest, EmptyWorkload) {
    const std::vector<Process> empty_workload;

    const auto result = ProcessValidator::validate(empty_workload);

    EXPECT_FALSE(result.is_valid());
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error_count(), 1);
    EXPECT_TRUE(has_error_code(result, ValidationErrorCode::EmptyWorkload));
}

// 3. Empty process ID
TEST(ProcessValidatorTest, EmptyProcessId) {
    const std::vector<Process> workload = {
        Process("", 0, 5, 1)
    };

    const auto result = ProcessValidator::validate(workload);

    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.error_count(), 1);
    EXPECT_TRUE(has_error_code(result, ValidationErrorCode::EmptyProcessId));
    EXPECT_EQ(result.errors().front().process_index, 0);
}

// 4. Whitespace-only process ID
TEST(ProcessValidatorTest, WhitespaceOnlyProcessId) {
    const std::vector<Process> workload = {
        Process("   \t \n ", 0, 5, 1)
    };

    const auto result = ProcessValidator::validate(workload);

    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.error_count(), 1);
    EXPECT_TRUE(has_error_code(result, ValidationErrorCode::EmptyProcessId));
}

// 5. Duplicate process IDs
TEST(ProcessValidatorTest, DuplicateProcessIds) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5, 1),
        Process("P2", 1, 4, 2),
        Process("P1", 2, 3, 3)
    };

    const auto result = ProcessValidator::validate(workload);

    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.error_count(), 1);
    EXPECT_TRUE(has_error_code(result, ValidationErrorCode::DuplicateProcessId));
    EXPECT_EQ(result.errors().front().process_index, 2);
}

// 6. Duplicate IDs with different letter casing
TEST(ProcessValidatorTest, DuplicateIdsWithDifferentLetterCasing) {
    const std::vector<Process> workload = {
        Process("JobAlpha", 0, 5, 1),
        Process("jobalpha", 2, 4, 2),
        Process("JOBALPHA", 3, 2, 3)
    };

    const auto result = ProcessValidator::validate(workload);

    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(count_error_code(result, ValidationErrorCode::DuplicateProcessId), 2);
}

// 7. Duplicate IDs with surrounding whitespace
TEST(ProcessValidatorTest, DuplicateIdsWithSurroundingWhitespace) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5, 1),
        Process("  P1  ", 2, 4, 2)
    };

    const auto result = ProcessValidator::validate(workload);

    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.error_count(), 1);
    EXPECT_TRUE(has_error_code(result, ValidationErrorCode::DuplicateProcessId));
}

// 8. Negative arrival time
TEST(ProcessValidatorTest, NegativeArrivalTime) {
    const std::vector<Process> workload = {
        Process("P1", -1, 5, 1)
    };

    const auto result = ProcessValidator::validate(workload);

    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.error_count(), 1);
    EXPECT_TRUE(has_error_code(result, ValidationErrorCode::NegativeArrivalTime));
}

// 9. Zero burst time
TEST(ProcessValidatorTest, ZeroBurstTime) {
    const std::vector<Process> workload = {
        Process("P1", 0, 0, 1)
    };

    const auto result = ProcessValidator::validate(workload);

    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.error_count(), 1);
    EXPECT_TRUE(has_error_code(result, ValidationErrorCode::NonPositiveBurstTime));
}

// 10. Negative burst time
TEST(ProcessValidatorTest, NegativeBurstTime) {
    const std::vector<Process> workload = {
        Process("P1", 0, -5, 1)
    };

    const auto result = ProcessValidator::validate(workload);

    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.error_count(), 1);
    EXPECT_TRUE(has_error_code(result, ValidationErrorCode::NonPositiveBurstTime));
}

// 11. Multiple simultaneous validation errors
TEST(ProcessValidatorTest, MultipleSimultaneousValidationErrors) {
    // Process 0 has negative arrival time AND zero burst time
    // Process 1 has empty ID AND negative burst time
    // Process 2 has duplicate ID of Process 3
    const std::vector<Process> workload = {
        Process("P1", -3, 0, 1),        // 2 errors: NegativeArrivalTime, NonPositiveBurstTime
        Process("", 2, -10, 2),          // 2 errors: EmptyProcessId, NonPositiveBurstTime
        Process("ProcX", 4, 6, 1),       // valid here
        Process("  procx ", 5, 8, 3)     // 1 error: DuplicateProcessId
    };

    const auto result = ProcessValidator::validate(workload);

    EXPECT_FALSE(result.is_valid());
    // Total errors = 2 + 2 + 0 + 1 = 5 errors
    EXPECT_EQ(result.error_count(), 5);

    EXPECT_EQ(count_error_code(result, ValidationErrorCode::NegativeArrivalTime), 1);
    EXPECT_EQ(count_error_code(result, ValidationErrorCode::NonPositiveBurstTime), 2);
    EXPECT_EQ(count_error_code(result, ValidationErrorCode::EmptyProcessId), 1);
    EXPECT_EQ(count_error_code(result, ValidationErrorCode::DuplicateProcessId), 1);

    // Summary contains descriptive error information
    const std::string summary = result.summary();
    EXPECT_FALSE(summary.empty());
    EXPECT_NE(summary.find("5 error(s)"), std::string::npos);
}

// 12. Valid zero arrival time
TEST(ProcessValidatorTest, ValidZeroArrivalTime) {
    const std::vector<Process> workload = {
        Process("P0", 0, 10, 0)
    };

    const auto result = ProcessValidator::validate(workload);

    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(result.error_count(), 0);
}

// 13. Valid positive burst time
TEST(ProcessValidatorTest, ValidPositiveBurstTime) {
    const std::vector<Process> workload = {
        Process("P1", 0, 1, 0),
        Process("P2", 1, 100, 5)
    };

    const auto result = ProcessValidator::validate(workload);

    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(result.error_count(), 0);
}

// Helper functions testing
TEST(ProcessValidatorTest, TrimWhitespaceHelper) {
    EXPECT_EQ(ProcessValidator::trim_whitespace("   abc   "), "abc");
    EXPECT_EQ(ProcessValidator::trim_whitespace("\t\r\nhello \t"), "hello");
    EXPECT_EQ(ProcessValidator::trim_whitespace(""), "");
    EXPECT_EQ(ProcessValidator::trim_whitespace("    "), "");
}

TEST(ProcessValidatorTest, NormalizeIdHelper) {
    EXPECT_EQ(ProcessValidator::normalize_id("  Process_A  "), "process_a");
    EXPECT_EQ(ProcessValidator::normalize_id("JOB-1"), "job-1");
}

TEST(ProcessValidatorTest, IsBlankHelper) {
    EXPECT_TRUE(ProcessValidator::is_blank(""));
    EXPECT_TRUE(ProcessValidator::is_blank("   "));
    EXPECT_TRUE(ProcessValidator::is_blank("\t\n\r"));
    EXPECT_FALSE(ProcessValidator::is_blank(" a "));
}

} // namespace cpu_scheduler::testing
