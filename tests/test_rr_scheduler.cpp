#include "cpu_scheduler/rr_scheduler.hpp"
#include "cpu_scheduler/gantt_chart.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <vector>

using namespace cpu_scheduler;

// ============================================================================
// Mandatory Scenarios A–H (Section 8)
// ============================================================================

TEST(RrSchedulerTest, ScenarioA_BasicRoundRobin) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 0, 3),
        Process("P3", 0, 1)
    };
    constexpr Time quantum = 2;

    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // Expected Gantt chart:
    // P1(0-2) -> P2(2-4) -> P3(4-5) -> P1(5-7) -> P2(7-8) -> P1(8-9)
    ASSERT_EQ(res.timeline().size(), 6u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 4, 5));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 5, 7));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P2", 7, 8));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P1", 8, 9));

    // Expected metrics:
    // P1: CT 9, TAT 9, WT 4, RT 0
    // P2: CT 8, TAT 8, WT 5, RT 2
    // P3: CT 5, TAT 5, WT 4, RT 4
    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 9);
    EXPECT_EQ(p1->turnaround_time, 9);
    EXPECT_EQ(p1->waiting_time, 4);
    EXPECT_EQ(p1->response_time, 0);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 8);
    EXPECT_EQ(p2->turnaround_time, 8);
    EXPECT_EQ(p2->waiting_time, 5);
    EXPECT_EQ(p2->response_time, 2);

    const auto* p3 = res.find_process_state("P3");
    ASSERT_NE(p3, nullptr);
    EXPECT_EQ(p3->completion_time, 5);
    EXPECT_EQ(p3->turnaround_time, 5);
    EXPECT_EQ(p3->waiting_time, 4);
    EXPECT_EQ(p3->response_time, 4);
}

TEST(RrSchedulerTest, ScenarioB_DifferentArrivalTimes) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 3),
        Process("P3", 4, 2)
    };
    constexpr Time quantum = 2;

    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // Expected Gantt chart:
    // P1(0-2) -> P2(2-4) -> P1(4-6) -> P3(6-8) -> P2(8-9) -> P1(9-10)
    ASSERT_EQ(res.timeline().size(), 6u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P3", 6, 8));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P2", 8, 9));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P1", 9, 10));

    // Verify metrics:
    // P1: CT 10, TAT 10, WT 5, RT 0
    // P2: CT 9,  TAT 7,  WT 4, RT 0
    // P3: CT 8,  TAT 4,  WT 2, RT 2
    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 10);
    EXPECT_EQ(p1->turnaround_time, 10);
    EXPECT_EQ(p1->waiting_time, 5);
    EXPECT_EQ(p1->response_time, 0);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 9);
    EXPECT_EQ(p2->turnaround_time, 7);
    EXPECT_EQ(p2->waiting_time, 4);
    EXPECT_EQ(p2->response_time, 0);

    const auto* p3 = res.find_process_state("P3");
    ASSERT_NE(p3, nullptr);
    EXPECT_EQ(p3->completion_time, 8);
    EXPECT_EQ(p3->turnaround_time, 4);
    EXPECT_EQ(p3->waiting_time, 2);
    EXPECT_EQ(p3->response_time, 2);
}

TEST(RrSchedulerTest, ScenarioC_CpuIdleTime) {
    const std::vector<Process> workload = {
        Process("P1", 3, 4),
        Process("P2", 10, 2)
    };
    constexpr Time quantum = 2;

    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // The two adjacent P1 allocations (3-5 and 5-7) must be normalized into one continuous segment:
    // IDLE(0-3) -> P1(3-7) -> IDLE(7-10) -> P2(10-12)
    ASSERT_EQ(res.timeline().size(), 4u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::idle(0, 3));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 3, 7));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::idle(7, 10));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 10, 12));

    EXPECT_EQ(res.total_execution_time(), 12);
    EXPECT_EQ(res.cpu_busy_time(), 6);
    EXPECT_EQ(res.cpu_idle_time(), 6);
    EXPECT_DOUBLE_EQ(res.cpu_utilization(), 50.0);
}

TEST(RrSchedulerTest, ScenarioD_ProcessArrivesExactlyAtQuantumExpiration) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 2)
    };
    constexpr Time quantum = 2;

    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // P1 runs 0-2. At 2, P2 arrives and is admitted BEFORE P1 is requeued.
    // P2 runs 2-4 and finishes.
    // P1 runs 4-6, then 6-7. Normalized into P1(4-7).
    // Expected: P1(0-2) -> P2(2-4) -> P1(4-7)
    ASSERT_EQ(res.timeline().size(), 3u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 7));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 7);
    EXPECT_EQ(p1->turnaround_time, 7);
    EXPECT_EQ(p1->waiting_time, 2);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 4);
    EXPECT_EQ(p2->turnaround_time, 2);
    EXPECT_EQ(p2->waiting_time, 0);
}

TEST(RrSchedulerTest, ScenarioE_ProcessCompletesExactlyAtQuantumExpiration) {
    const std::vector<Process> workload = {
        Process("P1", 0, 2),
        Process("P2", 0, 4)
    };
    constexpr Time quantum = 2;

    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // P1 finishes at 2 and is never requeued.
    // P2 runs 2-4, then 4-6. Normalized into P2(2-6).
    // Expected: P1(0-2) -> P2(2-6)
    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 6));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 2);
    EXPECT_EQ(p1->remaining_time, 0);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 6);
    EXPECT_EQ(p2->remaining_time, 0);
}

TEST(RrSchedulerTest, ScenarioF_SingleProcess) {
    const std::vector<Process> workload = {
        Process("P1", 0, 7)
    };
    constexpr Time quantum = 2;

    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // Single process runs continuously; adjacent segments normalized into one.
    ASSERT_EQ(res.timeline().size(), 1u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 7));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 7);
    EXPECT_EQ(p1->turnaround_time, 7);
    EXPECT_EQ(p1->waiting_time, 0);
    EXPECT_EQ(p1->response_time, 0);
}

TEST(RrSchedulerTest, ScenarioG_QuantumLargerThanEveryBurst) {
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 0, 2)
    };
    constexpr Time quantum = 10;

    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // Equivalent to FCFS:
    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 3, 5));
}

TEST(RrSchedulerTest, ScenarioH_QuantumEqualsBurstTime) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 0, 4)
    };
    constexpr Time quantum = 4;

    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 4));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 4, 8));

    for (const auto& s : res.processes()) {
        EXPECT_EQ(s.remaining_time, 0);
        EXPECT_EQ(s.status, ProcessExecutionStatus::Completed);
    }
}

// ============================================================================
// Quantum Validation Tests
// ============================================================================

TEST(RrSchedulerTest, ZeroQuantumRejected) {
    const std::vector<Process> workload = { Process("P1", 0, 5) };
    const auto outcome = RrScheduler::run(workload, 0);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(RrSchedulerTest, NegativeQuantumRejected) {
    const std::vector<Process> workload = { Process("P1", 0, 5) };
    const auto outcome = RrScheduler::run(workload, -3);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(RrSchedulerTest, QuantumOfOne) {
    const std::vector<Process> workload = {
        Process("P1", 0, 2),
        Process("P2", 0, 2)
    };
    // Q=1: P1(0-1) -> P2(1-2) -> P1(2-3) -> P2(3-4)
    const auto outcome = RrScheduler::run(workload, 1);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 1, 2));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 2, 3));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 3, 4));
}

TEST(RrSchedulerTest, VeryLargeValidQuantum) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 4)
    };
    constexpr Time large_q = 1'000'000;
    const auto outcome = RrScheduler::run(workload, large_q);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // Equivalent to FCFS
    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 5, 9));
}

// ============================================================================
// Process Ordering Tests
// ============================================================================

TEST(RrSchedulerTest, IdenticalArrivalTimesPreserveInputSequence) {
    const std::vector<Process> workload = {
        Process("A", 0, 2),
        Process("B", 0, 2),
        Process("C", 0, 2)
    };
    const auto outcome = RrScheduler::run(workload, 1);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // A(0-1), B(1-2), C(2-3), A(3-4), B(4-5), C(5-6)
    ASSERT_EQ(res.timeline().size(), 6u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("A", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("B", 1, 2));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("C", 2, 3));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("A", 3, 4));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("B", 4, 5));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("C", 5, 6));
}

TEST(RrSchedulerTest, MultipleArrivalsDuringExecutionSlice) {
    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 1, 2),
        Process("P3", 2, 2)
    };
    constexpr Time quantum = 3;

    // 0..3: P1 executes (rem 3). During this slice, P2 arrives at 1, P3 arrives at 2.
    // At 3, P2 and P3 are admitted before P1 is requeued.
    // Ready queue order at 3: P2, P3, P1.
    // 3..5: P2 executes (completes at 5).
    // 5..7: P3 executes (completes at 7).
    // 7..10: P1 executes remaining 3 (completes at 10).
    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 3, 5));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 5, 7));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 7, 10));
}

TEST(RrSchedulerTest, MultipleProcessesArrivingAtSameQuantumBoundary) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 2, 2),
        Process("P3", 2, 2)
    };
    constexpr Time quantum = 2;

    // At 2: P2 and P3 arrive simultaneously. Both admitted in input order (P2 then P3) before P1 requeued!
    // Queue at 2: P2, P3, P1.
    // 2..4: P2 finishes at 4.
    // 4..6: P3 finishes at 6.
    // 6..8: P1 finishes at 8.
    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 6, 8));
}

TEST(RrSchedulerTest, UnsortedWorkloadInputArrivalOrdering) {
    const std::vector<Process> workload = {
        Process("P3", 4, 2),
        Process("P1", 0, 2),
        Process("P2", 2, 2)
    };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 4, 6));
}

// ============================================================================
// Execution Behavior Tests
// ============================================================================

TEST(RrSchedulerTest, TwoProcessesAlternating) {
    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 0, 6)
    };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 6u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 6, 8));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 8, 10));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P2", 10, 12));
}

TEST(RrSchedulerTest, ProcessCompletingBeforeQuantumExpires) {
    const std::vector<Process> workload = {
        Process("P1", 0, 1),
        Process("P2", 0, 5)
    };
    constexpr Time quantum = 3;

    // P1 runs 0..1 (1 unit < Q=3) and completes!
    // P2 runs 1..4 (3 units, rem 2), then 4..6 (2 units).
    // Normalized P2: 1..6.
    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 1, 6));
}

TEST(RrSchedulerTest, ProcessRequiringManyQuanta) {
    const std::vector<Process> workload = {
        Process("Long", 0, 10),
        Process("Short", 0, 2)
    };
    constexpr Time quantum = 2;

    // Long(0-2) -> Short(2-4, done) -> Long(4-12, normalized)
    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("Long", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("Short", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("Long", 4, 12));
}

TEST(RrSchedulerTest, IntermediateIdlePeriodBetweenBatches) {
    const std::vector<Process> workload = {
        Process("P1", 0, 2),
        Process("P2", 0, 2),
        Process("P3", 10, 2)
    };
    constexpr Time quantum = 2;

    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::idle(4, 10));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P3", 10, 12));
}

TEST(RrSchedulerTest, MultipleIdleIntervals) {
    const std::vector<Process> workload = {
        Process("P1", 2, 2),
        Process("P2", 8, 2),
        Process("P3", 15, 2)
    };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 6u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::idle(0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::idle(4, 8));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 8, 10));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::idle(10, 15));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P3", 15, 17));

    EXPECT_EQ(res.cpu_idle_time(), 2 + 4 + 5);
    EXPECT_EQ(res.cpu_busy_time(), 6);
    EXPECT_EQ(res.total_execution_time(), 17);
}

// ============================================================================
// Metrics Verification Tests
// ============================================================================

TEST(RrSchedulerTest, CorrectResponseTimesPreservedAcrossQuanta) {
    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 0, 4)
    };
    constexpr Time quantum = 2;

    // P1 starts at 0 -> RT = 0.
    // P2 starts at 2 -> RT = 2.
    // Even though both execute multiple quanta, their response times must reflect their first start.
    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->start_time, 0);
    EXPECT_EQ(p1->response_time, 0);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->start_time, 2);
    EXPECT_EQ(p2->response_time, 2);
}

TEST(RrSchedulerTest, WaitingTimeAccumulatesAllWaitingPeriods) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 0, 4)
    };
    constexpr Time quantum = 2;

    // P1: executes 0-2 (waited 0), waits 2-4 (waited 2), executes 4-6. Total WT = 2.
    // P2: waits 0-2 (waited 2), executes 2-4 (waited 0), waits 4-6 (waited 2), executes 6-8. Total WT = 4.
    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 6);
    EXPECT_EQ(p1->turnaround_time, 6);
    EXPECT_EQ(p1->waiting_time, 2);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 8);
    EXPECT_EQ(p2->turnaround_time, 8);
    EXPECT_EQ(p2->waiting_time, 4);
}

TEST(RrSchedulerTest, AverageMetricsComputation) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 0, 3),
        Process("P3", 0, 1)
    };
    // From Scenario A:
    // P1: TAT 9, WT 4, RT 0
    // P2: TAT 8, WT 5, RT 2
    // P3: TAT 5, WT 4, RT 4
    // Avg TAT = (9 + 8 + 5) / 3 = 22.0 / 3.0
    // Avg WT  = (4 + 5 + 4) / 3 = 13.0 / 3.0
    // Avg RT  = (0 + 2 + 4) / 3 = 6.0 / 3.0 = 2.0
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_NEAR(res.average_turnaround_time(), 22.0 / 3.0, 1e-6);
    EXPECT_NEAR(res.average_waiting_time(), 13.0 / 3.0, 1e-6);
    EXPECT_NEAR(res.average_response_time(), 2.0, 1e-6);
}

TEST(RrSchedulerTest, CpuUtilizationCorrectness) {
    const std::vector<Process> workload = {
        Process("P1", 2, 4)
    };
    // Idle 0-2 (2), P1 2-6 (4). Total = 6, Busy = 4, Idle = 2.
    // Util = (4 / 6) * 100% = 66.6667%
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_EQ(res.total_execution_time(), 6);
    EXPECT_EQ(res.cpu_busy_time(), 4);
    EXPECT_EQ(res.cpu_idle_time(), 2);
    EXPECT_NEAR(res.cpu_utilization(), (4.0 / 6.0) * 100.0, 1e-6);
}

// ============================================================================
// Timeline Integrity Tests
// ============================================================================

TEST(RrSchedulerTest, ChronologicalOrderingAndNoOverlaps) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 1, 4),
        Process("P3", 3, 2)
    };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    for (std::size_t i = 1; i < res.timeline().size(); ++i) {
        EXPECT_EQ(res.timeline()[i - 1].end_time, res.timeline()[i].start_time);
        EXPECT_GT(res.timeline()[i].duration(), 0);
    }
}

TEST(RrSchedulerTest, EveryProcessCompletesExactlyOnce) {
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 5),
        Process("P3", 4, 3)
    };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    for (const auto& state : res.processes()) {
        EXPECT_EQ(state.status, ProcessExecutionStatus::Completed);
        EXPECT_EQ(state.remaining_time, 0);
        EXPECT_GT(state.completion_time, 0);
    }
}

TEST(RrSchedulerTest, SumOfExecutionSegmentsEqualsProcessBurstTime) {
    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 1, 5),
        Process("P3", 2, 3)
    };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    for (const auto& proc : workload) {
        Time executed = 0;
        for (const auto& seg : res.timeline()) {
            if (seg.is_process() && seg.process_id == proc.id) {
                executed += seg.duration();
            }
        }
        EXPECT_EQ(executed, proc.burst_time);
    }
}

TEST(RrSchedulerTest, GanttChartRenderingRoundRobin) {
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 0, 2)
    };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto chart = GanttChart::render(outcome.value());

    EXPECT_FALSE(chart.empty());
    EXPECT_NE(chart.find("P1"), std::string::npos);
    EXPECT_NE(chart.find("P2"), std::string::npos);
}

// ============================================================================
// Boundary, Validation & Overflow Tests
// ============================================================================

TEST(RrSchedulerTest, EmptyWorkloadRejected) {
    const std::vector<Process> workload = {};
    const auto outcome = RrScheduler::run(workload, 2);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(RrSchedulerTest, InvalidProcessIdRejected) {
    const std::vector<Process> workload = { Process("   ", 0, 5) };
    const auto outcome = RrScheduler::run(workload, 2);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(RrSchedulerTest, DuplicateProcessIdsRejected) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("p1", 2, 3)
    };
    const auto outcome = RrScheduler::run(workload, 2);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(RrSchedulerTest, NegativeArrivalTimeRejected) {
    const std::vector<Process> workload = { Process("P1", -2, 5) };
    const auto outcome = RrScheduler::run(workload, 2);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(RrSchedulerTest, ZeroBurstTimeRejected) {
    const std::vector<Process> workload = { Process("P1", 0, 0) };
    const auto outcome = RrScheduler::run(workload, 2);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(RrSchedulerTest, NegativeBurstTimeRejected) {
    const std::vector<Process> workload = { Process("P1", 0, -4) };
    const auto outcome = RrScheduler::run(workload, 2);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(RrSchedulerTest, LargeSimulationTimeValues) {
    constexpr Time large_arrival = 1'000'000'000'000LL;
    constexpr Time large_burst = 100;
    const std::vector<Process> workload = {
        Process("P1", large_arrival, large_burst)
    };
    const auto outcome = RrScheduler::run(workload, 10);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_EQ(res.total_execution_time(), large_arrival + large_burst);
    EXPECT_EQ(res.cpu_idle_time(), large_arrival);
    EXPECT_EQ(res.cpu_busy_time(), large_burst);
}

TEST(RrSchedulerTest, NearInt64OverflowSucceeds) {
    constexpr Time near_max = std::numeric_limits<Time>::max() - 1000;
    const std::vector<Process> workload = {
        Process("P1", near_max, 500)
    };
    const auto outcome = RrScheduler::run(workload, 100);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().total_execution_time(), near_max + 500);
}

TEST(RrSchedulerTest, ArithmeticOverflowReturnsExpectedError) {
    constexpr Time max_time = std::numeric_limits<Time>::max();
    const std::vector<Process> workload = {
        Process("P1", max_time - 10, 20)
    };
    const auto outcome = RrScheduler::run(workload, 15);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_overflow_error());
}

TEST(RrSchedulerTest, NoDivisionByZeroErrors) {
    const std::vector<Process> workload = { Process("P1", 0, 1) };
    const auto outcome = RrScheduler::run(workload, 1);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_GT(outcome.value().cpu_utilization(), 0.0);
}

// ============================================================================
// API Ergonomics & State Query Tests
// ============================================================================

TEST(RrSchedulerTest, DefaultConstructorAndGetter) {
    const RrScheduler scheduler;
    EXPECT_EQ(scheduler.time_quantum(), 2);
}

TEST(RrSchedulerTest, ExplicitConstructorAndGetter) {
    const RrScheduler scheduler(5);
    EXPECT_EQ(scheduler.time_quantum(), 5);
}

TEST(RrSchedulerTest, ScheduleWithQuantumOverride) {
    const RrScheduler scheduler(10);
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 0, 4)
    };
    // Quantum override of 2 instead of instance default 10
    const auto outcome = scheduler.schedule(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
}

TEST(RrSchedulerTest, FindProcessStateReturnsNullptrForUnknownId) {
    const std::vector<Process> workload = { Process("P1", 0, 5) };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().find_process_state("Unknown"), nullptr);
}

TEST(RrSchedulerTest, OriginalProcessObjectsRemainUnchanged) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5, 2),
        Process("P2", 2, 4, 1)
    };
    const auto backup = workload;

    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(workload, backup);
}

TEST(RrSchedulerTest, GanttChartDisplaysIdleIntervals) {
    const std::vector<Process> workload = { Process("P1", 4, 3) };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto chart = GanttChart::render(outcome.value());

    EXPECT_NE(chart.find("Idle"), std::string::npos);
    EXPECT_NE(chart.find("P1"), std::string::npos);
}

TEST(RrSchedulerTest, NormalizationOfThreeConsecutiveAllocations) {
    // Single process needing 3 full quanta (burst 6, Q=2)
    const std::vector<Process> workload = { Process("P1", 0, 6) };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 1u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 6));
}

TEST(RrSchedulerTest, FourProcessesRoundRobinInterleaving) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 0, 5),
        Process("P3", 0, 5),
        Process("P4", 0, 5)
    };
    constexpr Time quantum = 2;

    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // Round 1 (each 2): P1(0-2), P2(2-4), P3(4-6), P4(6-8)
    // Round 2 (each 2): P1(8-10), P2(10-12), P3(12-14), P4(14-16)
    // Round 3 (each 1): P1(16-17), P2(17-18), P3(18-19), P4(19-20)
    ASSERT_EQ(res.timeline().size(), 12u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P4", 6, 8));
    EXPECT_EQ(res.timeline()[11], ExecutionSegment::process("P4", 19, 20));
    EXPECT_EQ(res.total_execution_time(), 20);
}

TEST(RrSchedulerTest, ZeroGapHandoffBetweenProcesses) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 4, 3)
    };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // P1 runs 0-2, 2-4 (normalized to 0-4). P2 runs 4-6, 6-7 (normalized to 4-7).
    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 4));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 4, 7));
    EXPECT_EQ(res.cpu_idle_time(), 0);
}

TEST(RrSchedulerTest, QuantumLargerThanTotalWorkloadBurstSum) {
    const std::vector<Process> workload = {
        Process("P1", 0, 2),
        Process("P2", 0, 3)
    };
    constexpr Time quantum = 50;

    const auto outcome = RrScheduler::run(workload, quantum);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 5));
}

TEST(RrSchedulerTest, ProcessStateStatusCompletedForAll) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 1, 3)
    };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    for (const auto& s : outcome.value().processes()) {
        EXPECT_EQ(s.status, ProcessExecutionStatus::Completed);
        EXPECT_EQ(s.remaining_time, 0);
    }
}

TEST(RrSchedulerTest, FindProcessStateSuccessfulQuery) {
    const std::vector<Process> workload = {
        Process("ProcA", 0, 5),
        Process("ProcB", 0, 2)
    };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto* state = outcome.value().find_process_state("ProcB");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->process_id, "ProcB");
    EXPECT_EQ(state->start_time, 2);
    EXPECT_EQ(state->completion_time, 4);
    EXPECT_EQ(state->turnaround_time, 4);
    EXPECT_EQ(state->waiting_time, 2);
    EXPECT_EQ(state->response_time, 2);
}

TEST(RrSchedulerTest, NonZeroInitialArrivalWithoutIdleAfterwards) {
    const std::vector<Process> workload = {
        Process("P1", 5, 3),
        Process("P2", 5, 2)
    };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // Idle 0-5, P1 5-7, P2 7-9, P1 9-10
    ASSERT_EQ(res.timeline().size(), 4u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::idle(0, 5));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 5, 7));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 7, 9));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 9, 10));
}

TEST(RrSchedulerTest, TurnaroundEqualsWaitPlusBurstInvariant) {
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1),
        Process("P4", 5, 4)
    };
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    for (const auto& s : outcome.value().processes()) {
        EXPECT_EQ(s.turnaround_time, s.waiting_time + s.burst_time);
    }
}

TEST(RrSchedulerTest, TenProcessesRoundRobinInterleaving) {
    std::vector<Process> workload;
    workload.reserve(10);
    for (int i = 0; i < 10; ++i) {
        workload.emplace_back("P" + std::to_string(i + 1), 0, 3);
    }
    // 10 processes, burst 3 each, quantum 2:
    // Round 1: each runs 2 units (20 units total)
    // Round 2: each runs remaining 1 unit (10 units total)
    // Total execution = 30 units, 20 segments
    const auto outcome = RrScheduler::run(workload, 2);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 20u);
    EXPECT_EQ(res.total_execution_time(), 30);
    EXPECT_EQ(res.cpu_busy_time(), 30);
    EXPECT_EQ(res.cpu_idle_time(), 0);
}
