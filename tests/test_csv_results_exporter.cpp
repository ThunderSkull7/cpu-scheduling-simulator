#include "cpu_scheduler/csv_exporter.hpp"
#include "cpu_scheduler/fcfs_scheduler.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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

} // namespace
} // namespace cpu_scheduler
