#include <gtest/gtest.h>

#include "cpu_scheduler/execution_segment.hpp"
#include "cpu_scheduler/gantt_chart.hpp"
#include "cpu_scheduler/mlfq_scheduler.hpp"
#include "cpu_scheduler/process.hpp"
#include "cpu_scheduler/process_state.hpp"
#include "cpu_scheduler/process_validator.hpp"

#include <limits>
#include <string>
#include <vector>

using namespace cpu_scheduler;

// ============================================================================
// Group 1: Configuration and Validation Tests (14 tests)
// ============================================================================

TEST(MlfqValidationTest, LessThanTwoLevelsRejected) {
    MlfqConfig config;
    config.add_level(2); // Only 1 level

    const std::vector<Process> workload = { Process("P1", 0, 5) };
    const auto outcome = MlfqScheduler::run(workload, config);

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlfqValidationTest, ZeroLevelsRejected) {
    MlfqConfig config; // 0 levels

    const std::vector<Process> workload = { Process("P1", 0, 5) };
    const auto outcome = MlfqScheduler::run(workload, config);

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
}

TEST(MlfqValidationTest, TwoLevelsAccepted) {
    MlfqConfig config({2, 4});
    const std::vector<Process> workload = { Process("P1", 0, 3) };
    const auto outcome = MlfqScheduler::run(workload, config);

    EXPECT_TRUE(outcome.has_value());
}

TEST(MlfqValidationTest, MultipleLevelsAccepted) {
    MlfqConfig config({2, 4, 8, 16});
    const std::vector<Process> workload = { Process("P1", 0, 5) };
    const auto outcome = MlfqScheduler::run(workload, config);

    EXPECT_TRUE(outcome.has_value());
}

TEST(MlfqValidationTest, ZeroQuantumRejected) {
    MlfqConfig config;
    config.add_level(0).add_level(4);

    const std::vector<Process> workload = { Process("P1", 0, 5) };
    const auto outcome = MlfqScheduler::run(workload, config);

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
}

TEST(MlfqValidationTest, NegativeQuantumRejected) {
    MlfqConfig config;
    config.add_level(2).add_level(-4);

    const std::vector<Process> workload = { Process("P1", 0, 5) };
    const auto outcome = MlfqScheduler::run(workload, config);

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
}

TEST(MlfqValidationTest, NegativeAllotmentRejected) {
    MlfqConfig config;
    config.add_level(2, 2).add_level(4, -1);

    const std::vector<Process> workload = { Process("P1", 0, 5) };
    const auto outcome = MlfqScheduler::run(workload, config);

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
}

TEST(MlfqValidationTest, NegativeBoostIntervalRejected) {
    MlfqConfig config({2, 4}, -5);

    const std::vector<Process> workload = { Process("P1", 0, 5) };
    const auto outcome = MlfqScheduler::run(workload, config);

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
}

TEST(MlfqValidationTest, DisabledBoostIntervalNulloptAccepted) {
    MlfqConfig config({2, 4}, std::nullopt);
    EXPECT_FALSE(config.is_boost_enabled());

    const std::vector<Process> workload = { Process("P1", 0, 5) };
    const auto outcome = MlfqScheduler::run(workload, config);

    EXPECT_TRUE(outcome.has_value());
}

TEST(MlfqValidationTest, DisabledBoostIntervalZeroAccepted) {
    MlfqConfig config({2, 4}, 0);
    EXPECT_FALSE(config.is_boost_enabled());

    const std::vector<Process> workload = { Process("P1", 0, 5) };
    const auto outcome = MlfqScheduler::run(workload, config);

    EXPECT_TRUE(outcome.has_value());
}

TEST(MlfqValidationTest, EmptyWorkloadRejected) {
    MlfqConfig config({2, 4});
    const std::vector<Process> workload = {};
    const auto outcome = MlfqScheduler::run(workload, config);

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
}

TEST(MlfqValidationTest, InvalidProcessAttributesRejected) {
    MlfqConfig config({2, 4});
    const std::vector<Process> workload = { Process("P1", -1, 5) }; // Negative arrival
    const auto outcome = MlfqScheduler::run(workload, config);

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
}

TEST(MlfqValidationTest, DuplicateProcessIdsRejected) {
    MlfqConfig config({2, 4});
    const std::vector<Process> workload = { Process("P1", 0, 5), Process("p1", 1, 3) };
    const auto outcome = MlfqScheduler::run(workload, config);

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
}

TEST(MlfqValidationTest, FluentBuilderChaining) {
    MlfqConfig config;
    config.add_level(2, 4)
          .add_level(4, 8)
          .set_boost_interval(50);

    EXPECT_EQ(config.levels.size(), 2);
    EXPECT_EQ(config.levels[0].time_quantum, 2);
    EXPECT_EQ(config.levels[0].effective_allotment(), 4);
    EXPECT_EQ(config.levels[1].time_quantum, 4);
    EXPECT_EQ(config.levels[1].effective_allotment(), 8);
    EXPECT_TRUE(config.is_boost_enabled());
    EXPECT_EQ(config.boost_interval, 50);
}

// ============================================================================
// Group 2: Basic Scheduling and Demotion Tests (16 tests)
// ============================================================================

// Scenario A: Basic Demotion
// Config: Q0 = 2, Q1 = 4, Q2 = 8
// Process: P1 (Arrival 0, Burst 9)
// Expected: P1(0-2) -> P1(2-6) -> P1(6-9), Q0 -> Q1 -> Q2
TEST(MlfqSchedulingTest, ScenarioA_BasicDemotionProgression) {
    const MlfqConfig config({2, 4, 8});
    const std::vector<Process> workload = { Process("P1", 0, 9) };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 6));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 6, 9));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 9);
    EXPECT_EQ(p1->turnaround_time, 9);
    EXPECT_EQ(p1->waiting_time, 0);
    EXPECT_EQ(p1->response_time, 0);
    EXPECT_EQ(p1->queue_id, "Q2");
}

// Scenario B: Multiple processes and demotion
// Config: Q0 = 2, Q1 = 4, Q2 = 8
// P1: arr 0, burst 10
// P2: arr 1, burst 3
// P3: arr 2, burst 2
// Expected timeline: P1(0-2) -> P2(2-4) -> P3(4-6) -> P1(6-10) -> P2(10-11) -> P1(11-15)
TEST(MlfqSchedulingTest, ScenarioB_MultipleProcessesAndDemotion) {
    const MlfqConfig config({2, 4, 8});
    const std::vector<Process> workload = {
        Process("P1", 0, 10),
        Process("P2", 1, 3),
        Process("P3", 2, 2)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 6);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 6, 10));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P2", 10, 11));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P1", 11, 15));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 15);
    EXPECT_EQ(p1->turnaround_time, 15);
    EXPECT_EQ(p1->waiting_time, 5);
    EXPECT_EQ(p1->response_time, 0);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 11);
    EXPECT_EQ(p2->turnaround_time, 10);
    EXPECT_EQ(p2->waiting_time, 7);
    EXPECT_EQ(p2->response_time, 1);

    const auto* p3 = res.find_process_state("P3");
    ASSERT_NE(p3, nullptr);
    EXPECT_EQ(p3->completion_time, 6);
    EXPECT_EQ(p3->turnaround_time, 4);
    EXPECT_EQ(p3->waiting_time, 2);
    EXPECT_EQ(p3->response_time, 2);
}

TEST(MlfqSchedulingTest, SingleProcessFinishesInQ0BeforeQuantumExpires) {
    const MlfqConfig config({4, 8});
    const std::vector<Process> workload = { Process("P1", 0, 3) };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 1);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(res.find_process_state("P1")->queue_id, "Q0");
}

TEST(MlfqSchedulingTest, SingleProcessFinishesExactlyAtQ0Quantum) {
    const MlfqConfig config({3, 6});
    const std::vector<Process> workload = { Process("P1", 0, 3) };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 1);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(res.find_process_state("P1")->queue_id, "Q0");
}

TEST(MlfqSchedulingTest, ProcessFinishesInQ1AfterOneDemotion) {
    const MlfqConfig config({2, 5});
    const std::vector<Process> workload = { Process("P1", 0, 4) };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 4));
    EXPECT_EQ(res.find_process_state("P1")->queue_id, "Q1");
}

TEST(MlfqSchedulingTest, BottomLevelRoundRobinMultipleProcesses) {
    // Both processes demote to bottom queue (Q1) and alternate
    const MlfqConfig config({2, 2});
    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 0, 6)
    };

    // Q0 runs P1 [0..2], demoted to Q1
    // Q0 runs P2 [2..4], demoted to Q1
    // Q1 has [P1, P2]. P1 runs [4..6] (rem 2). P2 runs [6..8] (rem 2).
    // P1 runs [8..10] (finishes). P2 runs [10..12] (finishes).
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 6);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 6, 8));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 8, 10));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P2", 10, 12));
}

TEST(MlfqSchedulingTest, ProcessReachingBottomQueueStaysAtBottomQueue) {
    const MlfqConfig config({1, 1}); // Q0, Q1
    const std::vector<Process> workload = { Process("P1", 0, 4) };

    // P1: Q0 runs [0-1] (demoted to Q1).
    // In Q1, runs [1-2], [2-3], [3-4] (stays in Q1).
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.find_process_state("P1")->queue_id, "Q1");
}

TEST(MlfqSchedulingTest, MultipleSimultaneousArrivalsAtTimeZero) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 2),
        Process("P2", 0, 2),
        Process("P3", 0, 2)
    };

    // All finish in Q0 in input order
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 4, 6));
}

TEST(MlfqSchedulingTest, UnsortedWorkloadArrivalSequence) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P2", 3, 2),
        Process("P1", 0, 2)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::idle(2, 3));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 3, 5));
}

TEST(MlfqSchedulingTest, FourQueueLevelsDemotion) {
    const MlfqConfig config({1, 2, 4, 8});
    const std::vector<Process> workload = { Process("P1", 0, 10) };

    // Q0 [0-1] (rem 9) -> Q1 [1-3] (rem 7) -> Q2 [3-7] (rem 3) -> Q3 [7-10] (finishes)
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 1, 3));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 3, 7));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 7, 10));
    EXPECT_EQ(res.find_process_state("P1")->queue_id, "Q3");
}

TEST(MlfqSchedulingTest, InitialCpuIdleInterval) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = { Process("P1", 5, 3) };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::idle(0, 5));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 5, 7));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 7, 8));
    EXPECT_EQ(res.cpu_idle_time(), 5);
    EXPECT_EQ(res.cpu_busy_time(), 3);
    EXPECT_EQ(res.total_execution_time(), 8);
}

TEST(MlfqSchedulingTest, IntermediateCpuIdleIntervalBetweenJobs) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 2),
        Process("P2", 6, 2)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::idle(2, 6));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 6, 8));
}

TEST(MlfqSchedulingTest, CpuUtilizationCalculatedAccurately) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 5, 3)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_EQ(res.cpu_busy_time(), 6);
    EXPECT_EQ(res.cpu_idle_time(), 2);
    EXPECT_EQ(res.total_execution_time(), 8);
    EXPECT_NEAR(res.cpu_utilization(), 75.0, 1e-6);
}

TEST(MlfqSchedulingTest, AverageMetricsComputedCorrectly) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 2), // CT=2, TAT=2, WT=0, RT=0
        Process("P2", 0, 2)  // CT=4, TAT=4, WT=2, RT=2
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_DOUBLE_EQ(res.average_turnaround_time(), 3.0);
    EXPECT_DOUBLE_EQ(res.average_waiting_time(), 1.0);
    EXPECT_DOUBLE_EQ(res.average_response_time(), 1.0);
}

TEST(MlfqSchedulingTest, GanttChartRendersSegments) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = { Process("P1", 0, 3) };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const std::string chart = GanttChart::render(outcome.value());
    EXPECT_FALSE(chart.empty());
    EXPECT_NE(chart.find("P1"), std::string::npos);
}

TEST(MlfqSchedulingTest, WorkloadObjectsRemainUnmodified) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = { Process("P1", 0, 5, 10) };
    const auto outcome = MlfqScheduler::run(workload, config);

    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(workload[0].id, "P1");
    EXPECT_EQ(workload[0].arrival_time, 0);
    EXPECT_EQ(workload[0].burst_time, 5);
    EXPECT_EQ(workload[0].priority, 10);
}

// ============================================================================
// Group 3: Preemption and Allotment Preservation Tests (16 tests)
// ============================================================================

// Scenario C: Higher-priority arrival preemption
// Q0 = 2, Q1 = 4, Q2 = 8
// P1: arr 0, burst 10
// P2: arr 5, burst 2
// Timeline: P1(0-2) -> P1(2-5) -> P2(5-7) -> P1(7-8) -> P1(8-12)
TEST(MlfqPreemptionTest, ScenarioC_HigherPriorityArrivalPreemption) {
    const MlfqConfig config({2, 4, 8});
    const std::vector<Process> workload = {
        Process("P1", 0, 10),
        Process("P2", 5, 2)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 5);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 5));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 5, 7));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 7, 8));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 8, 12));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 12);
    EXPECT_EQ(p1->turnaround_time, 12);
    EXPECT_EQ(p1->waiting_time, 2);
    EXPECT_EQ(p1->response_time, 0);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 7);
    EXPECT_EQ(p2->turnaround_time, 2);
    EXPECT_EQ(p2->waiting_time, 0);
    EXPECT_EQ(p2->response_time, 0);
}

// Scenario D: CPU allotment preservation across preemption
// Q0 = 2, Q1 = 4, Q2 = 8
// P1 runs Q0 [0-2], demoted to Q1 (allotment 4)
// In Q1, P1 runs 1 unit [2-3], remaining allotment 3
// P2 arrives in Q0 at t=3, runs [3-5]
// P1 resumes in Q1 at t=5 with remaining allotment 3, runs [5-8], demoted to Q2
// In Q2, P1 runs [8-10]
TEST(MlfqPreemptionTest, ScenarioD_CpuAllotmentPreservedAcrossInterruption) {
    const MlfqConfig config({2, 4, 8});
    const std::vector<Process> workload = {
        Process("P1", 0, 8),
        Process("P2", 3, 2)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 5);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 3));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 3, 5));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 5, 8));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 8, 10));
}

TEST(MlfqPreemptionTest, MultiplePreemptionsOfLowerLevelProcess) {
    const MlfqConfig config({2, 4, 8});
    const std::vector<Process> workload = {
        Process("P_long", 0, 10),
        Process("P_short1", 3, 1),
        Process("P_short2", 5, 1)
    };

    // P_long runs Q0 [0-2] (demoted Q1).
    // In Q1, P_long runs [2-3] (rem Q1 allotment 3).
    // P_short1 arrives at 3 in Q0, runs [3-4].
    // P_long resumes Q1 [4-5] (rem Q1 allotment 2).
    // P_short2 arrives at 5 in Q0, runs [5-6].
    // P_long resumes Q1 [6-8] (uses remaining 2 units of Q1 allotment, demoted Q2).
    // P_long runs Q2 [8-12].
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 7);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P_long", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P_long", 2, 3));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P_short1", 3, 4));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P_long", 4, 5));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P_short2", 5, 6));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P_long", 6, 8));
    EXPECT_EQ(res.timeline()[6], ExecutionSegment::process("P_long", 8, 12));
}

TEST(MlfqPreemptionTest, EqualLevelArrivalDoesNotPreemptRunningProcess) {
    const MlfqConfig config({4, 8});
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 1, 2)
    };

    // P2 arrives at 1 in Q0, but P1 is running in Q0 (equal level) -> P1 continues until 4!
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 4));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 4, 6));
}

TEST(MlfqPreemptionTest, ResponseTimeRecordedAtFirstDispatchOnly) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 1, 1)
    };

    // P1 starts at 0, runs [0-2], demoted Q1. P2 ran [2-3]. P1 resumes [3-6].
    // P1 start_time is 0, response time is 0 - 0 = 0!
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto* p1 = outcome.value().find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->response_time, 0);
}

TEST(MlfqPreemptionTest, ArrivalAtExactAllotmentExpirationBoundary) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 2, 2) // arrives exactly when P1 finishes Q0 quantum
    };

    // At 2, P1 exhausted Q0 allotment -> demoted to Q1.
    // P2 arrives in Q0 at 2 -> runs in Q0 [2-4].
    // P1 runs in Q1 [4-6].
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 6));
}

TEST(MlfqPreemptionTest, PreemptionNearQuantumBoundary) {
    const MlfqConfig config({4, 8});
    const std::vector<Process> workload = {
        Process("P1", 0, 10),
        Process("P2", 3, 2) // P1 ran 0..2 in Q0 (demoted Q1), in Q1 runs 2..3, P2 arrives at 3
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // P1: Q0 runs [0-4] (demoted Q1). In Q1 runs [4-6].
    // Wait: P2 arrived at 3 in Q0!
    // Since P1 was running in Q0 at time 3, P2 joined Q0 tail! P1 finishes its Q0 quantum at 4!
    // At 4: P1 demoted to Q1. P2 in Q0 runs [4-6]! Then P1 runs in Q1 [6-12]!
    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 4));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 4, 6));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 6, 12));
}

TEST(MlfqPreemptionTest, BottomLevelInterruptedAndResumes) {
    const MlfqConfig config({2, 4}); // Q0, Q1
    const std::vector<Process> workload = {
        Process("P_low", 0, 8), // demotes to Q1 at 2
        Process("P_high", 3, 2) // arrives at 3 in Q0
    };

    // P_low runs Q0 [0-2], demoted Q1.
    // In Q1, P_low runs [2-3] (rem burst 5, rem Q1 allotment 3).
    // P_high runs Q0 [3-5] (finishes).
    // P_low resumes Q1 [5-8] (rem burst 2, rem Q1 allotment 0 -> stays in Q1).
    // P_low runs Q1 [8-10] (finishes).
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 5);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P_low", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P_low", 2, 3));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P_high", 3, 5));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P_low", 5, 8));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P_low", 8, 10));
}

TEST(MlfqPreemptionTest, MultipleProcessesWaitingInLowerQueuesWhileQ0Executes) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 4), // demotes to Q1 at 2
        Process("P2", 0, 4), // demotes to Q1 at 4
        Process("P3", 4, 2)  // arrives at 4 in Q0
    };

    // Q0 has P1, P2.
    // P1 runs [0-2], demoted Q1.
    // P2 runs [2-4], demoted Q1.
    // At 4, P3 arrives in Q0! Q0 has priority over Q1!
    // P3 runs [4-6].
    // Then Q1 runs P1 [6-8] (finishes), P2 [8-10] (finishes).
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 5);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 6, 8));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P2", 8, 10));
}

TEST(MlfqPreemptionTest, ExactRemainingBurstPreserved) {
    const MlfqConfig config({2, 4, 8});
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 3, 1)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto* p1 = outcome.value().find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 8);
    EXPECT_EQ(p1->waiting_time, 1);
}

TEST(MlfqPreemptionTest, NoArtificialZeroDurationSegments) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 2),
        Process("P2", 2, 2)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    for (const auto& seg : outcome.value().timeline()) {
        EXPECT_GT(seg.end_time, seg.start_time);
    }
}

TEST(MlfqPreemptionTest, TotalBusyTimeEqualsSumOfBursts) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 3),
        Process("P3", 4, 2)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().cpu_busy_time(), 10);
}

TEST(MlfqPreemptionTest, ContinuousExecutionWithoutGaps) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 1, 3)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& tl = outcome.value().timeline();
    for (std::size_t i = 1; i < tl.size(); ++i) {
        EXPECT_EQ(tl[i].start_time, tl[i - 1].end_time);
    }
}

TEST(MlfqPreemptionTest, TurnaroundAndWaitingRelationshipHolds) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 2, 4)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    for (const auto& s : outcome.value().processes()) {
        EXPECT_EQ(s.turnaround_time, s.completion_time - s.arrival_time);
        EXPECT_EQ(s.waiting_time, s.turnaround_time - s.burst_time);
    }
}

TEST(MlfqPreemptionTest, LargeProcessInterruptedMultipleTimesAcrossLevels) {
    const MlfqConfig config({2, 4, 8});
    const std::vector<Process> workload = {
        Process("P_big", 0, 15),
        Process("P_int1", 3, 1),
        Process("P_int2", 7, 1)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().find_process_state("P_big")->completion_time, 17);
}

TEST(MlfqPreemptionTest, AllotmentDoesNotResetOnHigherPriorityPreemption) {
    // P1 has 4 allotment in Q1. Executes 3 units. Preempted.
    // Upon resumption, it MUST only execute 1 unit before demotion to Q2!
    const MlfqConfig config({2, 4, 8});
    const std::vector<Process> workload = {
        Process("P1", 0, 8),
        Process("P2", 5, 2)
    };

    // P1: Q0 [0-2] (demoted Q1).
    // Q1: [2-5] (runs 3 units, rem allotment 1).
    // P2: Q0 [5-7] (finishes).
    // P1 resumes Q1: runs for 1 unit [7-8] -> allotment expires -> demoted to Q2!
    // Q2: [8-10] (finishes).
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 5);
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 7, 8)); // Exactly 1 unit in Q1!
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 8, 10)); // Demoted to Q2!
}

// ============================================================================
// Group 4: Priority Boosting Tests (18 tests)
// ============================================================================

// Scenario E: Periodic priority boost promotes Q2 process to Q0
// Q0 = 2, Q1 = 4, Q2 = 8. Boost interval = 10.
// P1: arr 0, burst 16
// P2: arr 0, burst 4
// Expected: P1(0-2) -> P2(2-4) -> P1(4-8) -> P2(8-10) -> P1(10-12) -> P1(12-16) -> P1(16-20)
TEST(MlfqBoostingTest, ScenarioE_PeriodicPriorityBoostPromotesToQ0) {
    const MlfqConfig config({2, 4, 8}, 10);
    const std::vector<Process> workload = {
        Process("P1", 0, 16),
        Process("P2", 0, 4)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 7);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 8));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 8, 10));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 10, 12));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P1", 12, 16));
    EXPECT_EQ(res.timeline()[6], ExecutionSegment::process("P1", 16, 20));

    EXPECT_EQ(res.find_process_state("P1")->completion_time, 20);
    EXPECT_EQ(res.find_process_state("P2")->completion_time, 10);
}

// Scenario F: Boost occurs during process execution
// Q0 = 2, Q1 = 4, Q2 = 8. Boost interval = 5.
// P1: arr 0, burst 10
// Expected: P1(0-2) -> P1(2-5) -> P1(5-7) -> P1(7-10)
TEST(MlfqBoostingTest, ScenarioF_BoostInterruptsExecutingProcessAtExactTimestamp) {
    const MlfqConfig config({2, 4, 8}, 5);
    const std::vector<Process> workload = { Process("P1", 0, 10) };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 5)); // Interrupted at boost time 5!
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 5, 7)); // Resumes from Q0!
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 7, 10));
}

// Scenario G: Arrival exactly at boost timestamp
// Q0 = 2, Q1 = 4, Q2 = 8. Boost interval = 10.
// P1: arr 0, burst 14
// P2: arr 10, burst 2
// Expected: P1(0-2) -> P1(2-6) -> P1(6-10) -> P1(10-12) -> P2(12-14) -> P1(14-16)
TEST(MlfqBoostingTest, ScenarioG_ArrivalExactlyAtBoostTimestamp) {
    const MlfqConfig config({2, 4, 8}, 10);
    const std::vector<Process> workload = {
        Process("P1", 0, 14),
        Process("P2", 10, 2)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 6);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 6));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 6, 10));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 10, 12)); // P1 earlier arrival (0 < 10)
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P2", 12, 14));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P1", 14, 16));
}

// Scenario H: Simultaneous completion, demotion, and boost
// Q0 = 2, Q1 = 4. Boost interval = 4.
// P1: arr 0, burst 4
// P2: arr 2, burst 2
// Timeline: P1(0-2) -> P2(2-4) -> P1(4-6)
TEST(MlfqBoostingTest, ScenarioH_SimultaneousCompletionAndBoost) {
    const MlfqConfig config({2, 4}, 4);
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 2, 2)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4)); // Completes at 4
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 6)); // P1 boosted to Q0 and runs
    EXPECT_EQ(res.find_process_state("P2")->completion_time, 4);
}

// Scenario I: Starvation prevention under continuous Q0 arrival stream
TEST(MlfqBoostingTest, ScenarioI_StarvationPreventionWithBoost) {
    // Q0 = 2, Q1 = 4.
    // P_long (AT=0, BT=10) demotes to Q1 at t=2.
    // Three short processes arrive: P_s1 (AT=2, BT=2), P_s2 (AT=4, BT=2), P_s3 (AT=6, BT=2).
    // Without boost, P_long would wait until time 8.
    // With boost interval = 5:
    // Boost fires at t=5. P_long is boosted to Q0!
    // P_s2 (arr 4) and P_long (arr 0) in Q0. P_long has earlier arrival (0 < 4) so P_long runs at 5!
    const MlfqConfig config({2, 4}, 5);
    const std::vector<Process> workload = {
        Process("P_long", 0, 10),
        Process("P_s1", 2, 2),
        Process("P_s2", 4, 2)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // P_long runs [0-2] (demoted Q1)
    // P_s1 runs [2-4] (finishes)
    // P_s2 arrives at 4, runs [4-5] (interrupted by boost at 5!)
    // At 5, boost fires: P_long (AT 0) and P_s2 (AT 4) in Q0. P_long runs at 5!
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P_long", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P_s1", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P_s2", 4, 5));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P_long", 5, 7)); // Starvation avoided!
}

TEST(MlfqBoostingTest, DisabledBoostLeavesProcessesInLowerQueues) {
    const MlfqConfig config({2, 4}, std::nullopt);
    const std::vector<Process> workload = { Process("P1", 0, 6) };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 6));
    EXPECT_EQ(res.find_process_state("P1")->queue_id, "Q1");
}

TEST(MlfqBoostingTest, MultipleConsecutiveBoostsOverTime) {
    const MlfqConfig config({2, 4}, 4); // Boost every 4 units
    const std::vector<Process> workload = { Process("P1", 0, 8) };

    // P1 runs Q0 [0-2], demoted Q1.
    // Runs Q1 [2-4]. Boost at 4!
    // Promoted Q0: runs [4-6], demoted Q1.
    // Runs Q1 [6-8]. Finishes at 8.
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 6, 8));
}

TEST(MlfqBoostingTest, BoostWhileCpuIsIdle) {
    const MlfqConfig config({2, 4}, 5);
    const std::vector<Process> workload = {
        Process("P1", 0, 2),
        Process("P2", 8, 2)
    };

    // P1 finishes at 2. CPU idle 2..8. Boost at 5 occurs during idle.
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_EQ(res.cpu_idle_time(), 6);
    EXPECT_EQ(res.find_process_state("P2")->completion_time, 10);
}

TEST(MlfqBoostingTest, CompletedProcessesAreNeverBoosted) {
    const MlfqConfig config({2, 4}, 5);
    const std::vector<Process> workload = {
        Process("P1", 0, 2), // completes at 2
        Process("P2", 0, 6)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto* p1 = outcome.value().find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 2); // Remained 2, never re-executed
}

TEST(MlfqBoostingTest, AllotmentsResetOnBoost) {
    const MlfqConfig config({2, 4}, 6);
    const std::vector<Process> workload = { Process("P1", 0, 6) };

    // P1 runs Q0 [0-2], demoted Q1 (allotment 4).
    // In Q1, runs [2-6] (used 4 units). Boost at 6 promotes P1 to Q0 with fresh allotment 2!
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().total_execution_time(), 6);
}

TEST(MlfqBoostingTest, DeterministicOrderingAfterBoostEarlierWorkloadIndexTieBreaker) {
    const MlfqConfig config({2, 4}, 3);
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 0, 5)
    };

    // Both arrived at 0. P1 runs [0-2], P2 runs [2-3] (boost at 3).
    // Boost at 3: both in Q0. Both have arrival 0. P1 has index 0 < index 1 $\implies$ P1 runs first!
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_EQ(res.timeline()[0].process_id, "P1");
    EXPECT_EQ(res.timeline()[1].process_id, "P2");
    EXPECT_EQ(res.timeline()[2].process_id, "P1"); // P1 runs after boost
}

TEST(MlfqBoostingTest, BottomLevelProcessesPromotedCleanly) {
    const MlfqConfig config({1, 1, 1}, 5); // 3 levels, all quantum 1
    const std::vector<Process> workload = { Process("P1", 0, 6) };

    // P1: Q0 [0-1] -> Q1 [1-2] -> Q2 [2-3], [3-4], [4-5]. Boost at 5! Promoted to Q0!
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P1", 5, 6)); // in Q0!
}

TEST(MlfqBoostingTest, LargeBoostIntervalNeverFiresWhenWorkloadShort) {
    const MlfqConfig config({2, 4}, 1000);
    const std::vector<Process> workload = { Process("P1", 0, 4) };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().total_execution_time(), 4);
}

TEST(MlfqBoostingTest, BoostAtExactCompletionTimestamp) {
    const MlfqConfig config({2, 4}, 4);
    const std::vector<Process> workload = { Process("P1", 0, 4) };

    // P1 finishes at 4, exactly when boost is scheduled.
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().find_process_state("P1")->completion_time, 4);
}

TEST(MlfqBoostingTest, AllQueuesClearedUponBoost) {
    const MlfqConfig config({2, 4, 8}, 4);
    const std::vector<Process> workload = {
        Process("P1", 0, 10),
        Process("P2", 0, 10)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().cpu_busy_time(), 20);
}

TEST(MlfqBoostingTest, NearLimitBoostIntervalSafeAddition) {
    // Large boost interval does not cause arithmetic overflow exception
    const MlfqConfig config({2, 4}, std::numeric_limits<Time>::max() - 10);
    const std::vector<Process> workload = { Process("P1", 0, 4) };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
}

// ============================================================================
// Group 5: Event and Boundary Testing (15 tests)
// ============================================================================

// Scenario J: Multiple lower-level processes in 3 levels
TEST(MlfqEventTest, ScenarioJ_MultipleLowerLevelProcesses) {
    const MlfqConfig config({1, 2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 0, 4),
        Process("P3", 0, 3)
    };

    // Q0 has P1, P2, P3. Quantum 1.
    // P1 runs [0-1] (rem 4, demoted Q1)
    // P2 runs [1-2] (rem 3, demoted Q1)
    // P3 runs [2-3] (rem 2, demoted Q1)
    // Q1 has [P1, P2, P3]. Quantum 2.
    // P1 runs [3-5] (rem 2, demoted Q2)
    // P2 runs [5-7] (rem 1, demoted Q2)
    // P3 runs [7-9] (rem 0, finishes at 9)
    // Q2 has [P1, P2]. Quantum 4.
    // P1 runs [9-11] (finishes at 11)
    // P2 runs [11-12] (finishes at 12)
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 8);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 1, 2));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 2, 3));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 3, 5));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P2", 5, 7));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P3", 7, 9));
    EXPECT_EQ(res.timeline()[6], ExecutionSegment::process("P1", 9, 11));
    EXPECT_EQ(res.timeline()[7], ExecutionSegment::process("P2", 11, 12));

    EXPECT_EQ(res.find_process_state("P3")->completion_time, 9);
    EXPECT_EQ(res.find_process_state("P1")->completion_time, 11);
    EXPECT_EQ(res.find_process_state("P2")->completion_time, 12);
}

TEST(MlfqEventTest, FiveProcessesSimultaneousArrival) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 1),
        Process("P2", 0, 1),
        Process("P3", 0, 1),
        Process("P4", 0, 1),
        Process("P5", 0, 1)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 5);
    EXPECT_EQ(res.timeline()[0].process_id, "P1");
    EXPECT_EQ(res.timeline()[1].process_id, "P2");
    EXPECT_EQ(res.timeline()[2].process_id, "P3");
    EXPECT_EQ(res.timeline()[3].process_id, "P4");
    EXPECT_EQ(res.timeline()[4].process_id, "P5");
}

TEST(MlfqEventTest, SimultaneousCompletionAndArrival) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 3), // runs 0..2 in Q0, 2..3 in Q1 (finishes at 3)
        Process("P2", 3, 2)  // arrives exactly at 3 in Q0
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 3));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 3, 5));
}

TEST(MlfqEventTest, CustomAllotmentLargerThanQuantum) {
    MlfqConfig config;
    // Level 0: quantum = 2, but allotment = 4 (takes 2 quantums to demote)
    config.add_level(2, 4).add_level(4, 4);

    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 0, 2)
    };

    // Q0 has P1, P2.
    // P1 runs [0-2] (used 2 of 4 allotment, remaining allotment 2). Not demoted!
    // P1 requeues in Q0! Q0 has [P2, P1].
    // P2 runs [2-4] (finishes at 4).
    // P1 runs [4-6] (uses remaining 2 units of Q0 allotment, demoted Q1).
    // In Q1, P1 runs [6-8] (finishes).
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 6, 8));
}

TEST(MlfqEventTest, InstanceScheduleMethodMatchesStaticRun) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = { Process("P1", 0, 3) };

    MlfqScheduler scheduler(config);
    const auto res1 = scheduler.schedule(workload);
    const auto res2 = MlfqScheduler::run(workload, config);

    ASSERT_TRUE(res1.has_value());
    ASSERT_TRUE(res2.has_value());
    EXPECT_EQ(res1.value().timeline(), res2.value().timeline());
}

TEST(MlfqEventTest, VectorAndSpanOverloadsProduceIdenticalResults) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = { Process("P1", 0, 3), Process("P2", 1, 2) };

    const auto res1 = MlfqScheduler::run(workload, config);
    const auto res2 = MlfqScheduler::run(std::span<const Process>(workload), config);

    ASSERT_TRUE(res1.has_value());
    ASSERT_TRUE(res2.has_value());
    EXPECT_EQ(res1.value().timeline(), res2.value().timeline());
}

TEST(MlfqEventTest, LargeBurstExecution) {
    const MlfqConfig config({10, 20});
    const std::vector<Process> workload = { Process("P1", 0, 1000) };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().find_process_state("P1")->completion_time, 1000);
}

TEST(MlfqEventTest, LargeArrivalTimestamps) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 1000000, 5)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().find_process_state("P1")->completion_time, 1000005);
}

TEST(MlfqEventTest, ProcessStatesPreservedInResult) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("ProcA", 0, 2),
        Process("ProcB", 1, 3)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.processes().size(), 2);
    EXPECT_EQ(res.processes()[0].process_id, "ProcA");
    EXPECT_EQ(res.processes()[1].process_id, "ProcB");
}

TEST(MlfqEventTest, TimelineSegmentsAreStrictlyChronological) {
    const MlfqConfig config({2, 4, 8});
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 4),
        Process("P3", 4, 2)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& tl = outcome.value().timeline();

    for (std::size_t i = 0; i < tl.size(); ++i) {
        EXPECT_LT(tl[i].start_time, tl[i].end_time);
        if (i > 0) {
            EXPECT_LE(tl[i - 1].end_time, tl[i].start_time);
        }
    }
}

TEST(MlfqEventTest, RepeatedDeterministicExecution) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 1, 3)
    };

    const auto res1 = MlfqScheduler::run(workload, config);
    const auto res2 = MlfqScheduler::run(workload, config);

    ASSERT_TRUE(res1.has_value());
    ASSERT_TRUE(res2.has_value());
    EXPECT_EQ(res1.value().timeline(), res2.value().timeline());
}

TEST(MlfqEventTest, IdleFastForwardBetweenBatches) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 0, 2),
        Process("P2", 10, 2),
        Process("P3", 20, 2)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().cpu_idle_time(), 16);
    EXPECT_EQ(outcome.value().cpu_busy_time(), 6);
    EXPECT_EQ(outcome.value().total_execution_time(), 22);
}

TEST(MlfqEventTest, BoostWithNoProcessesRunningDoesNotCrash) {
    const MlfqConfig config({2, 4}, 2); // Boost every 2 units
    const std::vector<Process> workload = {
        Process("P1", 10, 2) // Arrives late at 10
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().find_process_state("P1")->completion_time, 12);
}

TEST(MlfqEventTest, TotalExecutionTimeEqualsBusyPlusIdle) {
    const MlfqConfig config({2, 4});
    const std::vector<Process> workload = {
        Process("P1", 2, 4),
        Process("P2", 8, 3)
    };

    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();
    EXPECT_EQ(res.total_execution_time(), res.cpu_busy_time() + res.cpu_idle_time());
}

TEST(MlfqEventTest, AllotmentExpirationAtBottomLevelRequeuesAtBackOfBottomLevel) {
    const MlfqConfig config({2, 2}); // Q0, Q1
    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 0, 6)
    };

    // Both demoted to Q1 at t=4. In Q1, P1 runs 4..6, rotates to back of Q1.
    // P2 runs 6..8, rotates to back of Q1.
    // P1 runs 8..10 (finishes).
    // P2 runs 10..12 (finishes).
    const auto outcome = MlfqScheduler::run(workload, config);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 6);
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 6, 8));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 8, 10));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P2", 10, 12));
}
