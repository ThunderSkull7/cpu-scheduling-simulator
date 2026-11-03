#include "cpu_scheduler/fcfs_scheduler.hpp"
#include "cpu_scheduler/mlq_scheduler.hpp"
#include "cpu_scheduler/priority_scheduler.hpp"
#include "cpu_scheduler/results_renderer.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

namespace cpu_scheduler {
namespace {

TEST(ResultsRendererTest, RenderSummaryContainsExpectedFields) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 2, 6)
    };

    const auto req = SchedulingRequest::fcfs(workload);
    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const std::string summary = ResultsRenderer::render_summary(req, outcome.value());
    EXPECT_NE(summary.find("SIMULATION SUMMARY"), std::string::npos);
    EXPECT_NE(summary.find("First Come, First Served"), std::string::npos);
    EXPECT_NE(summary.find("Process Count:            2"), std::string::npos);
    EXPECT_NE(summary.find("Total Simulation Time:    10"), std::string::npos);
    EXPECT_NE(summary.find("CPU Busy Time:            10"), std::string::npos);
    EXPECT_NE(summary.find("CPU Idle Time:            0"), std::string::npos);
    EXPECT_NE(summary.find("CPU Utilization:          100.00%"), std::string::npos);
}

TEST(ResultsRendererTest, RenderProcessTableStandardColumns) {
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 1, 4)
    };

    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const std::string table = ResultsRenderer::render_process_table(outcome.value(), false, false);
    EXPECT_NE(table.find("Process Execution Details:"), std::string::npos);
    EXPECT_NE(table.find("PID"), std::string::npos);
    EXPECT_NE(table.find("Arrival"), std::string::npos);
    EXPECT_NE(table.find("Burst"), std::string::npos);
    EXPECT_NE(table.find("Completion"), std::string::npos);
    EXPECT_NE(table.find("Turnaround"), std::string::npos);
    EXPECT_NE(table.find("Waiting"), std::string::npos);
    EXPECT_NE(table.find("Response"), std::string::npos);
    EXPECT_EQ(table.find("Priority"), std::string::npos);
    EXPECT_EQ(table.find("Queue"), std::string::npos);

    EXPECT_NE(table.find("P1"), std::string::npos);
    EXPECT_NE(table.find("P2"), std::string::npos);
}

TEST(ResultsRendererTest, RenderProcessTableWithPriorityAndQueueColumns) {
    MlqWorkload mlq;
    mlq.add_queue(MlqQueueConfig("Q1", 0, QueueAlgorithm::Fcfs));
    mlq.add_process(Process("P1", 0, 3, 5), "Q1");

    auto outcome = MlqScheduler::run(mlq);
    ASSERT_TRUE(outcome.has_value());

    const std::string table = ResultsRenderer::render_process_table(outcome.value(), true, true);
    EXPECT_NE(table.find("Priority"), std::string::npos);
    EXPECT_NE(table.find("Queue"), std::string::npos);
    EXPECT_NE(table.find("P1"), std::string::npos);
    EXPECT_NE(table.find("Q1"), std::string::npos);
    EXPECT_NE(table.find("5"), std::string::npos);
}

TEST(ResultsRendererTest, RenderAggregateMetricsFormatting) {
    SchedulingMetrics metrics;
    metrics.average_turnaround_time = 7.33333;
    metrics.average_waiting_time = 3.66667;
    metrics.average_response_time = 2.0;
    metrics.cpu_utilization = 85.714;

    const std::string agg = ResultsRenderer::render_aggregate_metrics(metrics);
    EXPECT_NE(agg.find("AGGREGATE METRICS"), std::string::npos);
    EXPECT_NE(agg.find("Average Turnaround Time:    7.33"), std::string::npos);
    EXPECT_NE(agg.find("Average Waiting Time:       3.67"), std::string::npos);
    EXPECT_NE(agg.find("Average Response Time:      2.00"), std::string::npos);
    EXPECT_NE(agg.find("CPU Utilization:            85.71%"), std::string::npos);
}

TEST(ResultsRendererTest, RenderTimelineFormatsCorrectly) {
    const std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::idle(0, 2),
        ExecutionSegment::process("P1", 2, 5)
    };

    const std::string tl = ResultsRenderer::render_timeline(timeline);
    EXPECT_NE(tl.find("Execution Timeline:"), std::string::npos);
    EXPECT_NE(tl.find("[0 - 2: IDLE] [2 - 5: P1]"), std::string::npos);
}

TEST(ResultsRendererTest, RenderTimelineEmptyReturnsPlaceholder) {
    const std::vector<ExecutionSegment> empty;
    const std::string tl = ResultsRenderer::render_timeline(empty);
    EXPECT_NE(tl.find("[Empty timeline]"), std::string::npos);
}

TEST(ResultsRendererTest, RenderFullReportStringMatchesStreamOutput) {
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 2, 4)
    };

    const auto req = SchedulingRequest::fcfs(workload);
    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    RenderOptions options;
    options.show_summary = true;
    options.show_process_table = true;
    options.show_aggregate_metrics = true;
    options.show_gantt_chart = true;
    options.show_timeline = true;

    const std::string string_report = ResultsRenderer::render_full_report(req, outcome.value(), options);

    std::ostringstream stream;
    ResultsRenderer::render_full_report(stream, req, outcome.value(), options);

    EXPECT_EQ(string_report, stream.str());
    EXPECT_NE(string_report.find("SIMULATION SUMMARY"), std::string::npos);
    EXPECT_NE(string_report.find("Process Execution Details:"), std::string::npos);
    EXPECT_NE(string_report.find("AGGREGATE METRICS"), std::string::npos);
    EXPECT_NE(string_report.find("Gantt Chart:"), std::string::npos);
    EXPECT_NE(string_report.find("Execution Timeline:"), std::string::npos);
}

TEST(ResultsRendererTest, RenderFullReportSelectiveOptions) {
    const std::vector<Process> workload = { Process("P1", 0, 2) };
    const auto req = SchedulingRequest::fcfs(workload);
    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    // Only render Gantt Chart and summary, skip table and aggregate metrics
    RenderOptions options;
    options.show_summary = true;
    options.show_process_table = false;
    options.show_aggregate_metrics = false;
    options.show_gantt_chart = true;
    options.show_timeline = false;

    const std::string report = ResultsRenderer::render_full_report(req, outcome.value(), options);
    EXPECT_NE(report.find("SIMULATION SUMMARY"), std::string::npos);
    EXPECT_EQ(report.find("Process Execution Details:"), std::string::npos);
    EXPECT_EQ(report.find("AGGREGATE METRICS"), std::string::npos);
    EXPECT_NE(report.find("Gantt Chart:"), std::string::npos);
    EXPECT_EQ(report.find("Execution Timeline:"), std::string::npos);
}

} // namespace
} // namespace cpu_scheduler
