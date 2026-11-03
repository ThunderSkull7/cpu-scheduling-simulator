#include "cpu_scheduler/csv_exporter.hpp"
#include "cpu_scheduler/csv_parser.hpp"
#include "cpu_scheduler/results_renderer.hpp"
#include "cpu_scheduler/scheduling_engine.hpp"
#include "cpu_scheduler/workload_importer.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace cpu_scheduler {
namespace {

TEST(PipelineIntegrationTest, EndToEndCsvToSchedulerToRendererToCsvExport) {
    // 1. Raw CSV Content with Quoted Fields and Mixed Prioritization
    const std::string input_csv =
        "process_id,arrival_time,burst_time,priority\n"
        "\"Task, Alpha\",0,4,2\n"
        "\"Task, Beta\",1,5,1\n"
        "\"Task, Gamma\",3,2,3\n";

    // 2. Import via WorkloadImporter (which calls CsvParser)
    auto import_res = WorkloadImporter::import_from_string(input_csv);
    ASSERT_TRUE(import_res.success);
    ASSERT_EQ(import_res.processes.size(), 3u);
    EXPECT_EQ(import_res.processes[0].id, "Task, Alpha");
    EXPECT_EQ(import_res.processes[1].id, "Task, Beta");

    // 3. Dispatch via Unified Scheduling Engine
    auto req = SchedulingRequest::preemptive_priority(import_res.processes);
    auto outcome = SchedulingEngine::execute(req);
    ASSERT_TRUE(outcome.has_value());
    const auto& sched_result = outcome.value();

    // 4. Render Full Presentation Report via ResultsRenderer
    RenderOptions render_opts;
    render_opts.show_priority = true;
    render_opts.gantt_options.border_style = BorderStyle::Unicode;
    std::string report = ResultsRenderer::render_full_report(req, sched_result, render_opts);

    EXPECT_NE(report.find("SIMULATION SUMMARY"), std::string::npos);
    EXPECT_NE(report.find("Preemptive Priority"), std::string::npos);
    EXPECT_NE(report.find("Task, Alpha"), std::string::npos);
    EXPECT_NE(report.find("Process Execution Details:"), std::string::npos);
    EXPECT_NE(report.find("AGGREGATE METRICS"), std::string::npos);

    // 5. Export to CSV Files via CsvExporter
    std::filesystem::path metrics_file = "test_pipeline_metrics.csv";
    std::filesystem::path timeline_file = "test_pipeline_timeline.csv";

    std::error_code ec;
    std::filesystem::remove(metrics_file, ec);
    std::filesystem::remove(timeline_file, ec);

    auto exp_metrics_res = CsvExporter::export_process_metrics_to_file(metrics_file, sched_result, true);
    auto exp_timeline_res = CsvExporter::export_timeline_to_file(timeline_file, sched_result, true);

    ASSERT_TRUE(exp_metrics_res.success);
    ASSERT_TRUE(exp_timeline_res.success);

    // 6. Independent Re-Parse and Verification via CsvParser
    auto metrics_parse = CsvParser::parse_file(metrics_file);
    ASSERT_TRUE(metrics_parse.success);
    ASSERT_EQ(metrics_parse.rows.size(), 4u); // 1 header + 3 processes

    // Check Header
    EXPECT_EQ(metrics_parse.rows[0].fields[0], "process_id");
    EXPECT_EQ(metrics_parse.rows[0].fields[1], "arrival_time");
    EXPECT_EQ(metrics_parse.rows[0].fields[2], "burst_time");

    // Check Process Rows (Commas preserved!)
    EXPECT_EQ(metrics_parse.rows[1].fields[0], "Task, Alpha");
    EXPECT_EQ(metrics_parse.rows[2].fields[0], "Task, Beta");

    // Re-Parse Timeline
    auto timeline_parse = CsvParser::parse_file(timeline_file);
    ASSERT_TRUE(timeline_parse.success);
    EXPECT_FALSE(timeline_parse.rows.empty());
    EXPECT_EQ(timeline_parse.rows[0].fields[0], "start_time");
    EXPECT_EQ(timeline_parse.rows[0].fields[1], "end_time");
    EXPECT_EQ(timeline_parse.rows[0].fields[2], "process_id");

    // Clean up
    std::filesystem::remove(metrics_file, ec);
    std::filesystem::remove(timeline_file, ec);
}

TEST(PipelineIntegrationTest, FormulaInjectionDefenseEndToEndPipeline) {
    // Malicious spreadsheet formula prefixes embedded in Process IDs
    const std::string malicious_csv =
        "process_id,arrival_time,burst_time\n"
        "\"=cmd|' /C calc'!A0\",0,3\n"
        "\"@SUM(1, 2)\",1,4\n"
        "\"+10\",2,2\n";

    // Ingestion succeeds safely
    auto import_res = WorkloadImporter::import_from_string(malicious_csv);
    ASSERT_TRUE(import_res.success);
    ASSERT_EQ(import_res.processes.size(), 3u);

    // Execution succeeds through engine
    auto req = SchedulingRequest::fcfs(import_res.processes);
    auto outcome = SchedulingEngine::execute(req);
    ASSERT_TRUE(outcome.has_value());

    // In-memory data retains exact original characters
    EXPECT_EQ(outcome.value().processes()[0].process_id, "=cmd|' /C calc'!A0");
    EXPECT_EQ(outcome.value().processes()[1].process_id, "@SUM(1, 2)");
    EXPECT_EQ(outcome.value().processes()[2].process_id, "+10");

    // Export to CSV with formula injection protection active
    std::string exported_metrics = CsvExporter::export_process_metrics_to_string(outcome.value());
    std::string exported_timeline = CsvExporter::export_timeline_to_string(outcome.value());

    // Both files must have leading single quotes protecting against spreadsheet execution
    EXPECT_NE(exported_metrics.find("\"'=cmd|' /C calc'!A0\""), std::string::npos);
    EXPECT_NE(exported_metrics.find("\"'@SUM(1, 2)\""), std::string::npos);
    EXPECT_NE(exported_metrics.find("\"'+10\""), std::string::npos);

    EXPECT_NE(exported_timeline.find("\"'=cmd|' /C calc'!A0\""), std::string::npos);
    EXPECT_NE(exported_timeline.find("\"'@SUM(1, 2)\""), std::string::npos);

    // In-memory process objects remain completely unaffected after export
    EXPECT_EQ(outcome.value().processes()[0].process_id, "=cmd|' /C calc'!A0");
}

TEST(PipelineIntegrationTest, MlqWorkloadImportSchedulingAndTimelineMetadataPipeline) {
    const std::string mlq_csv =
        "process_id,arrival_time,burst_time,priority,queue_id\n"
        "Interactive_1,0,4,1,Q_High\n"
        "Interactive_2,1,3,2,Q_High\n"
        "Batch_1,0,8,0,Q_Low\n";

    auto import_res = WorkloadImporter::import_from_string(mlq_csv);
    ASSERT_TRUE(import_res.success);
    ASSERT_TRUE(import_res.has_queue_id);
    ASSERT_EQ(import_res.queue_bindings.size(), 3u);

    // Assemble into MlqWorkload
    MlqWorkload mlq;
    mlq.set_inter_queue_policy(InterQueuePolicy::FixedPriorityPreemptive)
       .add_queue(MlqQueueConfig("Q_High", 0, QueueAlgorithm::RoundRobin, 2))
       .add_queue(MlqQueueConfig("Q_Low", 1, QueueAlgorithm::Fcfs));

    for (std::size_t i = 0; i < import_res.processes.size(); ++i) {
        mlq.add_process(import_res.processes[i], import_res.queue_bindings[i]);
    }

    auto outcome = SchedulingEngine::execute(SchedulingRequest::mlq(mlq));
    ASSERT_TRUE(outcome.has_value());

    // Export timeline to CSV
    std::string timeline_csv = CsvExporter::export_timeline_to_string(outcome.value());
    EXPECT_NE(timeline_csv.find("start_time,end_time,process_id,queue_id"), std::string::npos);
    EXPECT_NE(timeline_csv.find("Interactive_1,Q_High"), std::string::npos);
    EXPECT_NE(timeline_csv.find("Batch_1,Q_Low"), std::string::npos);
}

TEST(PipelineIntegrationTest, LargeTimestampBoundaryPipelineNoOverflow) {
    // Valid large timestamps well within int64 bounds
    const std::string large_time_csv =
        "process_id,arrival_time,burst_time\n"
        "BigP1,1000000,500000\n"
        "BigP2,2000000,300000\n";

    auto import_res = WorkloadImporter::import_from_string(large_time_csv);
    ASSERT_TRUE(import_res.success);

    auto outcome = SchedulingEngine::execute(SchedulingRequest::fcfs(import_res.processes));
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    EXPECT_EQ(res.processes()[0].completion_time, 1500000);
    EXPECT_EQ(res.processes()[1].completion_time, 2300000);

    // Render summary and Gantt chart without overflow or assertion crash
    RenderOptions opts;
    opts.gantt_options.min_block_width = 8;
    std::string report = ResultsRenderer::render_full_report(
        SchedulingRequest::fcfs(import_res.processes), res, opts);
    EXPECT_NE(report.find("1500000"), std::string::npos);
    EXPECT_NE(report.find("2300000"), std::string::npos);
}

TEST(PipelineIntegrationTest, MalformedCsvRejectedTransactionallyWithoutEngineInvocation) {
    const std::string bad_csv =
        "process_id,arrival_time,burst_time\n"
        "P1,0,5\n"
        "P1,2,3\n"; // Duplicate PID!

    auto import_res = WorkloadImporter::import_from_string(bad_csv);
    EXPECT_FALSE(import_res.success);
    EXPECT_TRUE(import_res.processes.empty());

    // When import fails, client code must not invoke SchedulingEngine
    if (!import_res.success) {
        EXPECT_EQ(import_res.processes.size(), 0u);
    }
}

TEST(PipelineIntegrationTest, ResultsRendererAsciiAndUnicodeBorderStyles) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 2, 2)
    };
    auto outcome = SchedulingEngine::execute(SchedulingRequest::fcfs(workload));
    ASSERT_TRUE(outcome.has_value());

    // ASCII Mode
    GanttOptions ascii_opts;
    ascii_opts.border_style = BorderStyle::Ascii;
    std::string ascii_chart = ResultsRenderer::render_gantt_chart(outcome.value().timeline(), ascii_opts);
    EXPECT_NE(ascii_chart.find("+---"), std::string::npos);

    // Unicode Mode
    GanttOptions unicode_opts;
    unicode_opts.border_style = BorderStyle::Unicode;
    std::string unicode_chart = ResultsRenderer::render_gantt_chart(outcome.value().timeline(), unicode_opts);
    EXPECT_NE(unicode_chart.find("┌"), std::string::npos);
    EXPECT_NE(unicode_chart.find("┘"), std::string::npos);
}

TEST(PipelineIntegrationTest, ResultsRendererMultiRowWrappingOnLongSchedule) {
    // Construct a schedule that spans many slices exceeding 80 columns
    std::vector<Process> workload;
    for (int i = 1; i <= 15; ++i) {
        workload.emplace_back("LongProcess_" + std::to_string(i), 0, 3);
    }

    auto outcome = SchedulingEngine::execute(SchedulingRequest::round_robin(workload, 1));
    ASSERT_TRUE(outcome.has_value());

    GanttOptions opts;
    opts.max_row_width = 60; // Force wrapping
    std::string chart = ResultsRenderer::render_gantt_chart(outcome.value().timeline(), opts);

    // Confirm that the chart wrapped onto multiple Gantt blocks
    std::size_t top_border_count = 0;
    std::size_t pos = 0;
    while ((pos = chart.find("+---", pos)) != std::string::npos) {
        ++top_border_count;
        pos += 4;
    }
    EXPECT_GT(top_border_count, 1u);
}

} // namespace
} // namespace cpu_scheduler
