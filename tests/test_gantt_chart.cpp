#include "cpu_scheduler/fcfs_scheduler.hpp"
#include "cpu_scheduler/gantt_chart.hpp"
#include "cpu_scheduler/mlfq_scheduler.hpp"
#include "cpu_scheduler/mlq_scheduler.hpp"
#include "cpu_scheduler/priority_scheduler.hpp"
#include "cpu_scheduler/rr_scheduler.hpp"
#include "cpu_scheduler/sjf_scheduler.hpp"
#include "cpu_scheduler/srtf_scheduler.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace cpu_scheduler {
namespace {

TEST(EnhancedGanttChartTest, EmptyTimelineReturnsEmptyIndicator) {
    const std::vector<ExecutionSegment> empty;
    EXPECT_EQ(GanttChart::render(empty), "[Empty Timeline]\n");

    const SchedulingResult empty_result;
    EXPECT_EQ(GanttChart::render(empty_result), "[Empty Timeline]\n");
}

TEST(EnhancedGanttChartTest, SingleProcessExecution) {
    const std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::process("P1", 0, 5)
    };

    const std::string chart = GanttChart::render(timeline);
    EXPECT_NE(chart.find("P1"), std::string::npos);
    EXPECT_NE(chart.find("0"), std::string::npos);
    EXPECT_NE(chart.find("5"), std::string::npos);
    EXPECT_NE(chart.find("+"), std::string::npos);
    EXPECT_NE(chart.find("|"), std::string::npos);
}

TEST(EnhancedGanttChartTest, MultipleProcessesSequential) {
    const std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::process("P1", 0, 4),
        ExecutionSegment::process("P2", 4, 9),
        ExecutionSegment::process("P3", 9, 15)
    };

    const std::string chart = GanttChart::render(timeline);
    EXPECT_NE(chart.find("P1"), std::string::npos);
    EXPECT_NE(chart.find("P2"), std::string::npos);
    EXPECT_NE(chart.find("P3"), std::string::npos);
    EXPECT_NE(chart.find("0"), std::string::npos);
    EXPECT_NE(chart.find("4"), std::string::npos);
    EXPECT_NE(chart.find("9"), std::string::npos);
    EXPECT_NE(chart.find("15"), std::string::npos);
}

TEST(EnhancedGanttChartTest, IdleIntervalsLeadingIntermediateTrailing) {
    const std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::idle(0, 3),
        ExecutionSegment::process("P1", 3, 7),
        ExecutionSegment::idle(7, 10),
        ExecutionSegment::process("P2", 10, 14),
        ExecutionSegment::idle(14, 18)
    };

    const std::string chart = GanttChart::render(timeline);
    EXPECT_NE(chart.find("Idle"), std::string::npos);
    EXPECT_NE(chart.find("P1"), std::string::npos);
    EXPECT_NE(chart.find("P2"), std::string::npos);
    EXPECT_NE(chart.find("0"), std::string::npos);
    EXPECT_NE(chart.find("3"), std::string::npos);
    EXPECT_NE(chart.find("7"), std::string::npos);
    EXPECT_NE(chart.find("10"), std::string::npos);
    EXPECT_NE(chart.find("14"), std::string::npos);
    EXPECT_NE(chart.find("18"), std::string::npos);
}

TEST(EnhancedGanttChartTest, PreemptionAndRepeatedProcessSegments) {
    // SRTF preemption: P1 runs, P2 preempts, P1 resumes
    const std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::process("P1", 0, 2),
        ExecutionSegment::process("P2", 2, 5),
        ExecutionSegment::process("P1", 5, 9)
    };

    const std::string chart = GanttChart::render(timeline);
    EXPECT_NE(chart.find("P1"), std::string::npos);
    EXPECT_NE(chart.find("P2"), std::string::npos);
    EXPECT_NE(chart.find("0"), std::string::npos);
    EXPECT_NE(chart.find("2"), std::string::npos);
    EXPECT_NE(chart.find("5"), std::string::npos);
    EXPECT_NE(chart.find("9"), std::string::npos);
}

TEST(EnhancedGanttChartTest, RoundRobinCyclicSegments) {
    const std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::process("P1", 0, 2),
        ExecutionSegment::process("P2", 2, 4),
        ExecutionSegment::process("P1", 4, 6),
        ExecutionSegment::process("P2", 6, 7)
    };

    const std::string chart = GanttChart::render(timeline);
    EXPECT_NE(chart.find("P1"), std::string::npos);
    EXPECT_NE(chart.find("P2"), std::string::npos);
    EXPECT_NE(chart.find("0"), std::string::npos);
    EXPECT_NE(chart.find("2"), std::string::npos);
    EXPECT_NE(chart.find("4"), std::string::npos);
    EXPECT_NE(chart.find("6"), std::string::npos);
    EXPECT_NE(chart.find("7"), std::string::npos);
}

TEST(EnhancedGanttChartTest, LargeTimestampValuesSafelyFormatted) {
    const std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::process("WorkerA", 1000000, 1000050),
        ExecutionSegment::process("WorkerB", 1000050, 1000100)
    };

    const std::string chart = GanttChart::render(timeline);
    EXPECT_NE(chart.find("WorkerA"), std::string::npos);
    EXPECT_NE(chart.find("WorkerB"), std::string::npos);
    EXPECT_NE(chart.find("1000000"), std::string::npos);
    EXPECT_NE(chart.find("1000050"), std::string::npos);
    EXPECT_NE(chart.find("1000100"), std::string::npos);
}

TEST(EnhancedGanttChartTest, LargeDurationDifferencesProportionalScaling) {
    // P1 runs for 1 time unit, P2 runs for 50 time units
    const std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::process("P1", 0, 1),
        ExecutionSegment::process("P2", 1, 51)
    };

    GanttOptions opts;
    opts.min_block_width = 7;
    opts.max_block_width = 24;
    opts.proportional_scaling = true;

    const std::string chart = GanttChart::render(timeline, opts);
    EXPECT_NE(chart.find("P1"), std::string::npos);
    EXPECT_NE(chart.find("P2"), std::string::npos);
    EXPECT_NE(chart.find("0"), std::string::npos);
    EXPECT_NE(chart.find("1"), std::string::npos);
    EXPECT_NE(chart.find("51"), std::string::npos);
}

TEST(EnhancedGanttChartTest, LongProcessIdentifiersExpandWidth) {
    const std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::process("SuperLongProcessName_Alpha", 0, 10)
    };

    const std::string chart = GanttChart::render(timeline);
    EXPECT_NE(chart.find("SuperLongProcessName_Alpha"), std::string::npos);
    EXPECT_NE(chart.find("0"), std::string::npos);
    EXPECT_NE(chart.find("10"), std::string::npos);
}

TEST(EnhancedGanttChartTest, RowWrappingWhenExceedingMaxRowWidth) {
    // 10 segments of width ~7-8, exceeding standard 80 columns
    std::vector<ExecutionSegment> timeline;
    for (int i = 0; i < 10; ++i) {
        timeline.push_back(ExecutionSegment::process("P" + std::to_string(i), i * 5, (i + 1) * 5));
    }

    GanttOptions opts;
    opts.max_row_width = 50; // Force wrapping after ~4-5 segments

    const std::string chart = GanttChart::render(timeline, opts);
    EXPECT_NE(chart.find("P0"), std::string::npos);
    EXPECT_NE(chart.find("P9"), std::string::npos);

    // Multiple rows should result in multiple top borders '+'
    std::size_t top_border_count = 0;
    std::size_t pos = 0;
    while ((pos = chart.find("+-", pos)) != std::string::npos) {
        top_border_count++;
        pos += 2;
    }
    EXPECT_GE(top_border_count, 2u);
}

TEST(EnhancedGanttChartTest, UnicodeBorderStyleRendersCleanly) {
    const std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::process("P1", 0, 4),
        ExecutionSegment::process("P2", 4, 8)
    };

    GanttOptions opts;
    opts.border_style = BorderStyle::Unicode;

    const std::string chart = GanttChart::render(timeline, opts);
    EXPECT_NE(chart.find("P1"), std::string::npos);
    EXPECT_NE(chart.find("P2"), std::string::npos);
    // Should contain UTF-8 box characters
    EXPECT_NE(chart.find("\xE2\x94\x8C"), std::string::npos); // ┌
    EXPECT_NE(chart.find("\xE2\x94\x90"), std::string::npos); // ┐
}

TEST(EnhancedGanttChartTest, MlqQueueMetadataInBlocks) {
    MlqWorkload mlq;
    mlq.add_queue(MlqQueueConfig("Q_High", 0, QueueAlgorithm::Fcfs));
    mlq.add_queue(MlqQueueConfig("Q_Low", 1, QueueAlgorithm::Fcfs));
    mlq.add_process(Process("P1", 0, 3), "Q_High");
    mlq.add_process(Process("P2", 0, 4), "Q_Low");

    auto outcome = MlqScheduler::run(mlq);
    ASSERT_TRUE(outcome.has_value());

    GanttOptions opts;
    opts.show_queue_id = true;

    const std::string chart = GanttChart::render(outcome.value(), opts);
    EXPECT_NE(chart.find("P1 (Q_High)"), std::string::npos);
    EXPECT_NE(chart.find("P2 (Q_Low)"), std::string::npos);
}

TEST(EnhancedGanttChartTest, ProportionalScalingDisabledKeepsBaseWidths) {
    const std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::process("P1", 0, 2),
        ExecutionSegment::process("P2", 2, 20)
    };

    GanttOptions opts;
    opts.proportional_scaling = false;
    opts.min_block_width = 8;

    const std::string chart = GanttChart::render(timeline, opts);
    EXPECT_NE(chart.find("P1"), std::string::npos);
    EXPECT_NE(chart.find("P2"), std::string::npos);
}

} // namespace
} // namespace cpu_scheduler
