#include "cpu_scheduler/csv_exporter.hpp"
#include "cpu_scheduler/fcfs_scheduler.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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

TEST(CsvExporterTest, ExportProcessMetricsStringContent) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4, 1),
        Process("P2", 2, 3, 2)
    };

    auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    std::string csv = CsvExporter::export_process_metrics_to_string(outcome.value());
    EXPECT_NE(csv.find("process_id,arrival_time,burst_time,completion_time,turnaround_time,waiting_time,response_time,priority,queue_id"), std::string::npos);
    EXPECT_NE(csv.find("P1,0,4,4,4,0,0,1,"), std::string::npos);
    EXPECT_NE(csv.find("P2,2,3,7,5,2,2,2,"), std::string::npos);
}

TEST(CsvExporterTest, ExportTimelineStringContent) {
    const std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::idle(0, 2),
        ExecutionSegment::process("P1", 2, 6),
        ExecutionSegment::process("P2", 6, 9)
    };

    std::vector<ProcessState> states = {
        ProcessState::from_process(Process("P1", 2, 4)),
        ProcessState::from_process(Process("P2", 6, 3))
    };

    SchedulingMetrics metrics;
    SchedulingResult result(timeline, states, metrics);

    std::string csv = CsvExporter::export_timeline_to_string(result);
    EXPECT_NE(csv.find("start_time,end_time,process_id,queue_id"), std::string::npos);
    EXPECT_NE(csv.find("0,2,IDLE,"), std::string::npos);
    EXPECT_NE(csv.find("2,6,P1,"), std::string::npos);
    EXPECT_NE(csv.find("6,9,P2,"), std::string::npos);
}

TEST(CsvExporterTest, FormulaInjectionMitigationOnTextFields) {
    // Formula injection tests with =, +, -, @
    EXPECT_EQ(CsvExporter::sanitize_formula("=1+1"), "'=1+1");
    EXPECT_EQ(CsvExporter::sanitize_formula("+cmd|' /C calc'!A0"), "'+cmd|' /C calc'!A0");
    EXPECT_EQ(CsvExporter::sanitize_formula("-P1"), "'-P1");
    EXPECT_EQ(CsvExporter::sanitize_formula("@SUM(1,2)"), "'@SUM(1,2)");

    // Standard PID without formula characters is not touched
    EXPECT_EQ(CsvExporter::sanitize_formula("P1_normal"), "P1_normal");
    EXPECT_EQ(CsvExporter::sanitize_formula(""), "");
}

TEST(CsvExporterTest, EscapedFieldFormatting) {
    // Commas trigger quoting
    EXPECT_EQ(CsvExporter::escape_field("Doe, John"), "\"Doe, John\"");

    // Double quotes are doubled
    EXPECT_EQ(CsvExporter::escape_field("He said \"Hi\""), "\"He said \"\"Hi\"\"\"");

    // Formula sanitized fields are quoted because of leading apostrophe
    EXPECT_EQ(CsvExporter::escape_field("=cmd", true), "\"'=cmd\"");
}

TEST(CsvExporterTest, NumericFieldsNeverFormulaNeutralized) {
    // Create a result with negative priority (-5)
    std::vector<ProcessState> states;
    ProcessState s;
    s.process_id = "P1";
    s.arrival_time = 0;
    s.burst_time = 5;
    s.priority = -5; // negative integer
    s.completion_time = 5;
    s.turnaround_time = 5;
    s.waiting_time = 0;
    s.response_time = 0;
    states.push_back(s);

    SchedulingMetrics metrics;
    SchedulingResult result({}, states, metrics);

    std::string csv = CsvExporter::export_process_metrics_to_string(result);
    // Should have -5 as raw integer, NOT "'-5" or '-5
    EXPECT_NE(csv.find(",-5,"), std::string::npos);
    EXPECT_EQ(csv.find("'-5"), std::string::npos);
}

TEST(CsvExporterTest, InMemoryResultRemainsUnchangedAfterExport) {
    const std::vector<Process> workload = {
        Process("=MaliciousPID", 0, 4)
    };

    auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    // Before export
    EXPECT_EQ(outcome.value().processes()[0].process_id, "=MaliciousPID");

    // Perform export
    std::string csv = CsvExporter::export_process_metrics_to_string(outcome.value());
    EXPECT_NE(csv.find("\"'=MaliciousPID\""), std::string::npos);

    // After export: in-memory result PID is strictly preserved!
    EXPECT_EQ(outcome.value().processes()[0].process_id, "=MaliciousPID");
}

TEST(CsvExporterTest, FileExportWithParentDirectoryCreation) {
    const std::vector<Process> workload = { Process("P1", 0, 2) };
    auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    std::filesystem::path dir = "test_export_tmp_dir";
    std::filesystem::path file = dir / "metrics_test.csv";

    // Clean up if existed
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);

    auto res = CsvExporter::export_process_metrics_to_file(file, outcome.value(), true);
    EXPECT_TRUE(res.success);
    EXPECT_TRUE(std::filesystem::exists(file));

    // Verify file content
    std::ifstream in(file);
    std::string line;
    std::getline(in, line);
    EXPECT_NE(line.find("process_id"), std::string::npos);

    // Clean up
    in.close();
    std::filesystem::remove_all(dir, ec);
}

TEST(CsvExporterTest, FileExportOverwriteFalseFailsWhenTargetExists) {
    const std::vector<Process> workload = { Process("P1", 0, 2) };
    auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    std::filesystem::path file = "test_overwrite_check.csv";

    // Create file
    auto res1 = CsvExporter::export_process_metrics_to_file(file, outcome.value(), true);
    ASSERT_TRUE(res1.success);

    // Try exporting with overwrite=false
    auto res2 = CsvExporter::export_process_metrics_to_file(file, outcome.value(), false);
    EXPECT_FALSE(res2.success);
    EXPECT_NE(res2.error_message.find("Destination file already exists"), std::string::npos);

    // Clean up
    std::error_code ec;
    std::filesystem::remove(file, ec);
}


TEST(CsvExporterTest, FileExportToNewDestinationVerifiesContentAndNoTempLeftover) {
    const std::vector<Process> workload = { Process("P1", 0, 4) };
    auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    std::filesystem::path file = "test_export_new_dest.csv";
    std::filesystem::path temp_file = "test_export_new_dest.csv.tmp";

    std::error_code ec;
    std::filesystem::remove(file, ec);
    std::filesystem::remove(temp_file, ec);

    auto res = CsvExporter::export_process_metrics_to_file(file, outcome.value(), false);
    EXPECT_TRUE(res.success);
    EXPECT_TRUE(std::filesystem::exists(file));
    EXPECT_FALSE(std::filesystem::exists(temp_file));

    // Verify file content has header and process data
    std::ifstream in(file);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("process_id,arrival_time,burst_time"), std::string::npos);
    EXPECT_NE(content.find("P1,0,4,4,4,0,0"), std::string::npos);
    in.close();

    std::filesystem::remove(file, ec);
}

TEST(CsvExporterTest, FileExportOverwriteDisabledPreservesOriginalFileContent) {
    const std::vector<Process> workload = { Process("P1", 0, 3) };
    auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    std::filesystem::path file = "test_preserve_on_overwrite_false.csv";
    std::filesystem::path temp_file = "test_preserve_on_overwrite_false.csv.tmp";

    const std::string original_data = "PRESERVED_USER_DATA_DO_NOT_CORRUPT";
    {
        std::ofstream out(file);
        out << original_data;
    }

    auto res = CsvExporter::export_process_metrics_to_file(file, outcome.value(), false);
    EXPECT_FALSE(res.success);
    EXPECT_NE(res.error_message.find("Destination file already exists"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(temp_file));

    // Verify original content was NOT destroyed or overwritten
    std::ifstream in(file);
    std::string read_back;
    in >> read_back;
    EXPECT_EQ(read_back, original_data);
    in.close();

    std::error_code ec;
    std::filesystem::remove(file, ec);
}

TEST(CsvExporterTest, FileExportOverwriteEnabledReplacesContentCleanly) {
    const std::vector<Process> workload = { Process("P_NEW", 0, 5) };
    auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    std::filesystem::path file = "test_safe_overwrite_replace.csv";
    std::filesystem::path temp_file = "test_safe_overwrite_replace.csv.tmp";

    const std::string stale_data = "STALE_DATA_12345";
    {
        std::ofstream out(file);
        out << stale_data;
    }

    auto res = CsvExporter::export_process_metrics_to_file(file, outcome.value(), true);
    EXPECT_TRUE(res.success);
    EXPECT_FALSE(std::filesystem::exists(temp_file));

    // Verify file now contains new CSV content and no stale data
    std::ifstream in(file);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content.find(stale_data), std::string::npos);
    EXPECT_NE(content.find("P_NEW,0,5,5,5,0,0"), std::string::npos);
    in.close();

    std::error_code ec;
    std::filesystem::remove(file, ec);
}

TEST(CsvExporterTest, FileExportReplacementFailurePreservesDestinationAndCleansUpTemp) {
    const std::vector<Process> workload = { Process("P_FAIL", 0, 2) };
    auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    std::filesystem::path file = "test_replace_failure_guard.csv";
    std::filesystem::path temp_file = "test_replace_failure_guard.csv.tmp";

    const std::string original_data = "CRITICAL_UNTOUCHED_CONTENT";
    {
        std::ofstream out(file);
        out << original_data;
    }

#ifdef _WIN32
    // Mark the destination file read-only so ReplaceFileW fails deterministically with access denied
    DWORD old_attrs = GetFileAttributesW(file.c_str());
    ASSERT_NE(old_attrs, INVALID_FILE_ATTRIBUTES);
    SetFileAttributesW(file.c_str(), old_attrs | FILE_ATTRIBUTE_READONLY);

    auto res = CsvExporter::export_process_metrics_to_file(file, outcome.value(), true);

    // Replacement must fail
    EXPECT_FALSE(res.success);
    EXPECT_FALSE(res.error_message.empty());

    // Restore attributes before verification & cleanup
    SetFileAttributesW(file.c_str(), old_attrs);
#endif

    // Crucial check: Destination file still exists and its original content is strictly preserved!
    std::ifstream in(file);
    std::string read_back;
    in >> read_back;
    EXPECT_EQ(read_back, original_data);
    in.close();

    // Crucial check: Temporary file was deleted and not left behind
    EXPECT_FALSE(std::filesystem::exists(temp_file));

    std::error_code ec;
    std::filesystem::remove(file, ec);
}

TEST(CsvExporterTest, TimelineFileExportSafeReplacementAndContentVerification) {
    const std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::idle(0, 1),
        ExecutionSegment::process("P1", 1, 3)
    };
    std::vector<ProcessState> states = {
        ProcessState::from_process(Process("P1", 1, 2))
    };
    SchedulingMetrics metrics;
    SchedulingResult result(timeline, states, metrics);

    std::filesystem::path file = "test_timeline_safe_replace.csv";
    std::filesystem::path temp_file = "test_timeline_safe_replace.csv.tmp";

    const std::string stale_data = "OLD_TIMELINE_STALE";
    {
        std::ofstream out(file);
        out << stale_data;
    }

    auto res = CsvExporter::export_timeline_to_file(file, result, true);
    EXPECT_TRUE(res.success);
    EXPECT_FALSE(std::filesystem::exists(temp_file));

    std::ifstream in(file);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content.find(stale_data), std::string::npos);
    EXPECT_NE(content.find("start_time,end_time,process_id"), std::string::npos);
    EXPECT_NE(content.find("0,1,IDLE,"), std::string::npos);
    EXPECT_NE(content.find("1,3,P1,"), std::string::npos);
    in.close();

    std::error_code ec;
    std::filesystem::remove(file, ec);
}

TEST(CsvExporterTest, TimelineFileExportReplacementFailurePreservesDestination) {
    const std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::process("P1", 0, 2)
    };
    SchedulingResult result(timeline, {}, {});

    std::filesystem::path file = "test_timeline_replace_fail.csv";
    std::filesystem::path temp_file = "test_timeline_replace_fail.csv.tmp";

    const std::string original_data = "ORIGINAL_TIMELINE_DATA";
    {
        std::ofstream out(file);
        out << original_data;
    }

#ifdef _WIN32
    DWORD old_attrs = GetFileAttributesW(file.c_str());
    ASSERT_NE(old_attrs, INVALID_FILE_ATTRIBUTES);
    SetFileAttributesW(file.c_str(), old_attrs | FILE_ATTRIBUTE_READONLY);

    auto res = CsvExporter::export_timeline_to_file(file, result, true);
    EXPECT_FALSE(res.success);
    EXPECT_FALSE(res.error_message.empty());

    SetFileAttributesW(file.c_str(), old_attrs);
#endif

    std::ifstream in(file);
    std::string read_back;
    in >> read_back;
    EXPECT_EQ(read_back, original_data);
    in.close();

    EXPECT_FALSE(std::filesystem::exists(temp_file));

    std::error_code ec;
    std::filesystem::remove(file, ec);
}

} // namespace
} // namespace cpu_scheduler
