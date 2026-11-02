#include "cpu_scheduler/fcfs_scheduler.hpp"
#include "cpu_scheduler/gantt_chart.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <vector>

namespace cpu_scheduler::testing {

// ============================================================================
// Group 1: Basic Scheduling Scenarios (Tests 1 to 5)
// ============================================================================

TEST(FcfsSchedulerTest, SingleProcess) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5, 1)
    };

    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& result = outcome.value();
    ASSERT_EQ(result.timeline().size(), 1);

    const auto& seg = result.timeline().front();
    EXPECT_EQ(seg.type, SegmentType::Process);
    EXPECT_EQ(seg.process_id, "P1");
    EXPECT_EQ(seg.start_time, 0);
    EXPECT_EQ(seg.end_time, 5);

    const auto* p1 = result.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 5);
    EXPECT_EQ(p1->turnaround_time, 5);
    EXPECT_EQ(p1->waiting_time, 0);
    EXPECT_EQ(p1->response_time, 0);
}

TEST(FcfsSchedulerTest, TwoProcessesWithDifferentArrivalTimes) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 2, 6)
    };

    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& result = outcome.value();

    ASSERT_EQ(result.timeline().size(), 2);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P1", 0, 4));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P2", 4, 10));

    const auto* p1 = result.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 4);
    EXPECT_EQ(p1->turnaround_time, 4);
    EXPECT_EQ(p1->waiting_time, 0);

    const auto* p2 = result.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 10);
    EXPECT_EQ(p2->turnaround_time, 8); // 10 - 2
    EXPECT_EQ(p2->waiting_time, 2);    // 8 - 6
    EXPECT_EQ(p2->response_time, 2);   // 4 - 2
}

TEST(FcfsSchedulerTest, MultipleProcessesSimultaneousArrival) {
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 0, 5),
        Process("P3", 0, 2)
    };

    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& result = outcome.value();

    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P2", 3, 8));
    EXPECT_EQ(result.timeline()[2], ExecutionSegment::process("P3", 8, 10));
}

TEST(FcfsSchedulerTest, UnsortedInputSequence) {
    // Workload supplied in non-chronological arrival order
    const std::vector<Process> workload = {
        Process("P3", 6, 2),
        Process("P1", 0, 4),
        Process("P2", 3, 3)
    };

    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& result = outcome.value();

    // FCFS execution order must be P1 -> P2 -> P3
    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0].process_id, "P1");
    EXPECT_EQ(result.timeline()[1].process_id, "P2");
    EXPECT_EQ(result.timeline()[2].process_id, "P3");
}

TEST(FcfsSchedulerTest, StableInputOrderTieBreaking) {
    // Simultaneous arrival at time 5; original order A, B, C must be preserved
    const std::vector<Process> workload = {
        Process("ProcA", 5, 2),
        Process("ProcB", 5, 4),
        Process("ProcC", 5, 1)
    };

    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& result = outcome.value();

    ASSERT_EQ(result.timeline().size(), 4); // Idle 0..5, then ProcA, ProcB, ProcC
    EXPECT_TRUE(result.timeline()[0].is_idle());
    EXPECT_EQ(result.timeline()[1].process_id, "ProcA");
    EXPECT_EQ(result.timeline()[2].process_id, "ProcB");
    EXPECT_EQ(result.timeline()[3].process_id, "ProcC");
}

// ============================================================================
// Group 2: CPU Idle Scenarios (Tests 6 to 10)
// ============================================================================

TEST(FcfsSchedulerTest, FirstProcessArrivesAfterTimeZero) {
    const std::vector<Process> workload = {
        Process("P1", 4, 3)
    };

    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& result = outcome.value();

    ASSERT_EQ(result.timeline().size(), 2);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::idle(0, 4));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P1", 4, 7));

    EXPECT_EQ(result.cpu_idle_time(), 4);
    EXPECT_EQ(result.cpu_busy_time(), 3);
    EXPECT_EQ(result.total_execution_time(), 7);
}

TEST(FcfsSchedulerTest, IdleIntervalBetweenTwoProcesses) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 9, 3)
    };

    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& result = outcome.value();

    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::idle(5, 9));
    EXPECT_EQ(result.timeline()[2], ExecutionSegment::process("P2", 9, 12));

    EXPECT_EQ(result.cpu_idle_time(), 4);
}

TEST(FcfsSchedulerTest, MultipleIdleIntervals) {
    // Idle 0..3 -> P1 (3..7) -> Idle 7..10 -> P2 (10..12)
    const std::vector<Process> workload = {
        Process("P1", 3, 4),
        Process("P2", 10, 2)
    };

    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& result = outcome.value();

    ASSERT_EQ(result.timeline().size(), 4);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::idle(0, 3));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P1", 3, 7));
    EXPECT_EQ(result.timeline()[2], ExecutionSegment::idle(7, 10));
    EXPECT_EQ(result.timeline()[3], ExecutionSegment::process("P2", 10, 12));

    EXPECT_EQ(result.cpu_idle_time(), 6);
    EXPECT_EQ(result.cpu_busy_time(), 6);
    EXPECT_EQ(result.total_execution_time(), 12);
}

TEST(FcfsSchedulerTest, ProcessArrivingExactlyWhenPreviousProcessFinishes) {
    // P1 finishes at 6, P2 arrives at 6. No idle segment should be generated.
    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 6, 4)
    };

    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& result = outcome.value();

    ASSERT_EQ(result.timeline().size(), 2);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P1", 0, 6));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P2", 6, 10));
    EXPECT_EQ(result.cpu_idle_time(), 0);
}

TEST(FcfsSchedulerTest, NoUnnecessaryZeroLengthIdleSegments) {
    // Standard specification example from prompt:
    // P1: arr 0, burst 5 -> 0..5
    // P2: arr 2, burst 3 -> 5..8
    // P3: arr 4, burst 2 -> 8..10
    // P4: arr 10, burst 4 -> 10..14 (arrives at 10, previous finishes at 10)
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 3),
        Process("P3", 4, 2),
        Process("P4", 10, 4)
    };

    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& result = outcome.value();

    ASSERT_EQ(result.timeline().size(), 4);
    for (const auto& seg : result.timeline()) {
        EXPECT_GT(seg.duration(), 0);
        EXPECT_FALSE(seg.is_idle());
    }
}

// ============================================================================
// Group 3: Scheduling Metrics (Tests 11 to 19)
// ============================================================================

TEST(FcfsSchedulerTest, CorrectCompletionTimes) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 3),
        Process("P3", 4, 2),
        Process("P4", 10, 4)
    };

    const auto result = FcfsScheduler::run(workload).value();

    EXPECT_EQ(result.find_process_state("P1")->completion_time, 5);
    EXPECT_EQ(result.find_process_state("P2")->completion_time, 8);
    EXPECT_EQ(result.find_process_state("P3")->completion_time, 10);
    EXPECT_EQ(result.find_process_state("P4")->completion_time, 14);
}

TEST(FcfsSchedulerTest, CorrectTurnaroundTimes) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 3),
        Process("P3", 4, 2),
        Process("P4", 10, 4)
    };

    const auto result = FcfsScheduler::run(workload).value();

    EXPECT_EQ(result.find_process_state("P1")->turnaround_time, 5);  // 5 - 0
    EXPECT_EQ(result.find_process_state("P2")->turnaround_time, 6);  // 8 - 2
    EXPECT_EQ(result.find_process_state("P3")->turnaround_time, 6);  // 10 - 4
    EXPECT_EQ(result.find_process_state("P4")->turnaround_time, 4);  // 14 - 10
}

TEST(FcfsSchedulerTest, CorrectWaitingTimes) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 3),
        Process("P3", 4, 2),
        Process("P4", 10, 4)
    };

    const auto result = FcfsScheduler::run(workload).value();

    EXPECT_EQ(result.find_process_state("P1")->waiting_time, 0); // 5 - 5
    EXPECT_EQ(result.find_process_state("P2")->waiting_time, 3); // 6 - 3
    EXPECT_EQ(result.find_process_state("P3")->waiting_time, 4); // 6 - 2
    EXPECT_EQ(result.find_process_state("P4")->waiting_time, 0); // 4 - 4
}

TEST(FcfsSchedulerTest, CorrectResponseTimes) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 3),
        Process("P3", 4, 2),
        Process("P4", 10, 4)
    };

    const auto result = FcfsScheduler::run(workload).value();

    // For non-preemptive FCFS, RT must equal WT
    for (const auto& ps : result.processes()) {
        EXPECT_EQ(ps.response_time, ps.waiting_time);
    }
}

TEST(FcfsSchedulerTest, CorrectAverageWaitingTime) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 3),
        Process("P3", 4, 2),
        Process("P4", 10, 4)
    };

    const auto result = FcfsScheduler::run(workload).value();
    // Sum WT = 0 + 3 + 4 + 0 = 7. Avg = 7 / 4 = 1.75
    EXPECT_DOUBLE_EQ(result.average_waiting_time(), 1.75);
}

TEST(FcfsSchedulerTest, CorrectAverageTurnaroundTime) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 3),
        Process("P3", 4, 2),
        Process("P4", 10, 4)
    };

    const auto result = FcfsScheduler::run(workload).value();
    // Sum TAT = 5 + 6 + 6 + 4 = 21. Avg = 21 / 4 = 5.25
    EXPECT_DOUBLE_EQ(result.average_turnaround_time(), 5.25);
}

TEST(FcfsSchedulerTest, CorrectCpuBusyTime) {
    const std::vector<Process> workload = {
        Process("P1", 3, 4),
        Process("P2", 10, 2)
    };

    const auto result = FcfsScheduler::run(workload).value();
    // Busy = 4 + 2 = 6
    EXPECT_EQ(result.cpu_busy_time(), 6);
}

TEST(FcfsSchedulerTest, CorrectCpuIdleTime) {
    const std::vector<Process> workload = {
        Process("P1", 3, 4),
        Process("P2", 10, 2)
    };

    const auto result = FcfsScheduler::run(workload).value();
    // Idle = (0..3: 3) + (7..10: 3) = 6
    EXPECT_EQ(result.cpu_idle_time(), 6);
}

TEST(FcfsSchedulerTest, CorrectCpuUtilization) {
    const std::vector<Process> workload = {
        Process("P1", 3, 4),
        Process("P2", 10, 2)
    };

    const auto result = FcfsScheduler::run(workload).value();
    // Total = 12, Busy = 6 -> Utilization = 50%
    EXPECT_DOUBLE_EQ(result.cpu_utilization(), 50.0);
}

// ============================================================================
// Group 4: Boundary and Invalid Inputs (Tests 20 to 27)
// ============================================================================

TEST(FcfsSchedulerTest, EmptyWorkloadRejected) {
    const std::vector<Process> workload;

    const auto outcome = FcfsScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(FcfsSchedulerTest, InvalidProcessIdRejected) {
    const std::vector<Process> workload = {
        Process("   ", 0, 5)
    };

    const auto outcome = FcfsScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(FcfsSchedulerTest, DuplicateProcessIdsRejected) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("  p1 ", 2, 4)
    };

    const auto outcome = FcfsScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(FcfsSchedulerTest, NegativeArrivalTimeRejected) {
    const std::vector<Process> workload = {
        Process("P1", -2, 5)
    };

    const auto outcome = FcfsScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(FcfsSchedulerTest, ZeroBurstTimeRejected) {
    const std::vector<Process> workload = {
        Process("P1", 0, 0)
    };

    const auto outcome = FcfsScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(FcfsSchedulerTest, NegativeBurstTimeRejected) {
    const std::vector<Process> workload = {
        Process("P1", 0, -4)
    };

    const auto outcome = FcfsScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(FcfsSchedulerTest, LargeValidTimeValues) {
    constexpr Time large_arrival = 1'000'000'000'000LL;
    constexpr Time large_burst = 2'000'000'000'000LL;

    const std::vector<Process> workload = {
        Process("P1", large_arrival, large_burst)
    };

    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& result = outcome.value();

    EXPECT_EQ(result.find_process_state("P1")->completion_time, large_arrival + large_burst);
    EXPECT_EQ(result.cpu_idle_time(), large_arrival);
    EXPECT_EQ(result.cpu_busy_time(), large_burst);
}

TEST(FcfsSchedulerTest, TimeArithmeticOverflowProtection) {
    // Arrival time close to max int64 + burst time causes overflow
    const Time near_max = std::numeric_limits<Time>::max() - 5;
    const std::vector<Process> workload = {
        Process("P1", near_max, 100)
    };

    const auto outcome = FcfsScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_overflow_error());
    EXPECT_FALSE(outcome.error_message().empty());
}

// ============================================================================
// Group 5: Result Integrity (Tests 28 to 32)
// ============================================================================

TEST(FcfsSchedulerTest, OriginalProcessObjectsRemainUnchanged) {
    const std::vector<Process> workload = {
        Process("P1", 2, 5, 3),
        Process("P2", 0, 4, 1)
    };
    const std::vector<Process> copy = workload;

    const auto outcome = FcfsScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    // Workload vector must be identical to original copy
    EXPECT_EQ(workload, copy);
}

TEST(FcfsSchedulerTest, ExecutionSegmentsChronologicallyOrdered) {
    const std::vector<Process> workload = {
        Process("P3", 10, 2),
        Process("P1", 1, 4),
        Process("P2", 6, 3)
    };

    const auto result = FcfsScheduler::run(workload).value();
    const auto& timeline = result.timeline();

    ASSERT_GE(timeline.size(), 2);
    for (std::size_t i = 1; i < timeline.size(); ++i) {
        EXPECT_EQ(timeline[i - 1].end_time, timeline[i].start_time);
        EXPECT_LT(timeline[i].start_time, timeline[i].end_time);
    }
}

TEST(FcfsSchedulerTest, ExecutionSegmentsDoNotOverlap) {
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 2, 4),
        Process("P3", 4, 2)
    };

    const auto result = FcfsScheduler::run(workload).value();
    const auto& timeline = result.timeline();

    for (std::size_t i = 1; i < timeline.size(); ++i) {
        EXPECT_LE(timeline[i - 1].end_time, timeline[i].start_time);
    }
}

TEST(FcfsSchedulerTest, EveryValidProcessExecutesExactlyOnce) {
    const std::vector<Process> workload = {
        Process("P1", 0, 2),
        Process("P2", 1, 3),
        Process("P3", 2, 1)
    };

    const auto result = FcfsScheduler::run(workload).value();

    for (const auto& proc : workload) {
        std::size_t count = 0;
        for (const auto& seg : result.timeline()) {
            if (seg.is_process() && seg.process_id == proc.id) {
                ++count;
            }
        }
        EXPECT_EQ(count, 1);
        const auto* state = result.find_process_state(proc.id);
        ASSERT_NE(state, nullptr);
        EXPECT_EQ(state->status, ProcessExecutionStatus::Completed);
    }
}

TEST(FcfsSchedulerTest, SumOfProcessExecutionDurationsEqualsTotalCpuBusyTime) {
    const std::vector<Process> workload = {
        Process("P1", 1, 5),
        Process("P2", 4, 7),
        Process("P3", 15, 3)
    };

    const auto result = FcfsScheduler::run(workload).value();

    Time accumulated_process_duration = 0;
    for (const auto& seg : result.timeline()) {
        if (seg.is_process()) {
            accumulated_process_duration += seg.duration();
        }
    }

    EXPECT_EQ(accumulated_process_duration, result.cpu_busy_time());
    EXPECT_EQ(result.total_execution_time(), result.cpu_busy_time() + result.cpu_idle_time());
}

// ============================================================================
// Group 6: Gantt Chart Visualization
// ============================================================================

TEST(FcfsSchedulerTest, GanttChartRendering) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 8, 3)
    };

    const auto result = FcfsScheduler::run(workload).value();
    const std::string chart = GanttChart::render(result);

    EXPECT_FALSE(chart.empty());
    EXPECT_NE(chart.find("P1"), std::string::npos);
    EXPECT_NE(chart.find("Idle"), std::string::npos);
    EXPECT_NE(chart.find("P2"), std::string::npos);
    EXPECT_NE(chart.find("0"), std::string::npos);
    EXPECT_NE(chart.find("5"), std::string::npos);
    EXPECT_NE(chart.find("8"), std::string::npos);
    EXPECT_NE(chart.find("11"), std::string::npos);
}

TEST(FcfsSchedulerTest, GanttChartEmptyTimeline) {
    const std::vector<ExecutionSegment> empty_timeline;
    const std::string chart = GanttChart::render(empty_timeline);

    EXPECT_EQ(chart, "[Empty Timeline]\n");
}

} // namespace cpu_scheduler::testing
