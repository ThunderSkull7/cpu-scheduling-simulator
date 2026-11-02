#include "cpu_scheduler/srtf_scheduler.hpp"
#include "cpu_scheduler/gantt_chart.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <vector>

using namespace cpu_scheduler;

// ============================================================================
// Category A: Basic Scheduling
// ============================================================================

TEST(SrtfSchedulerTest, SingleProcess) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 1u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->start_time, 0);
    EXPECT_EQ(p1->completion_time, 5);
    EXPECT_EQ(p1->turnaround_time, 5);
    EXPECT_EQ(p1->waiting_time, 0);
    EXPECT_EQ(p1->response_time, 0);
}

TEST(SrtfSchedulerTest, TwoProcessesWithoutPreemption) {
    // P1 arrives at 0 with burst 2. P2 arrives at 3 with burst 4.
    // P1 finishes at 2. CPU idles 2..3. P2 runs 3..7.
    const std::vector<Process> workload = {
        Process("P1", 0, 2),
        Process("P2", 3, 4)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::idle(2, 3));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 3, 7));
}

TEST(SrtfSchedulerTest, TwoProcessesWhereSecondPreemptsFirst) {
    // P1 arrives at 0 with burst 10. P2 arrives at 2 with burst 2.
    // At time 2, P1 remaining is 8. P2 burst is 2 < 8 -> P2 preempts P1.
    // P2 runs 2..4. Then P1 resumes 4..12.
    const std::vector<Process> workload = {
        Process("P1", 0, 10),
        Process("P2", 2, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 12));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->start_time, 0);
    EXPECT_EQ(p1->completion_time, 12);
    EXPECT_EQ(p1->turnaround_time, 12);
    EXPECT_EQ(p1->waiting_time, 2);
    EXPECT_EQ(p1->response_time, 0);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->start_time, 2);
    EXPECT_EQ(p2->completion_time, 4);
    EXPECT_EQ(p2->turnaround_time, 2);
    EXPECT_EQ(p2->waiting_time, 0);
    EXPECT_EQ(p2->response_time, 0);
}

TEST(SrtfSchedulerTest, ThreeProcessesWithMultiplePreemptions) {
    // P1(0, 10), P2(1, 4), P3(2, 1)
    // 0..1: P1 runs (remaining 9)
    // 1: P2 arrives (burst 4 < 9) -> preempts P1
    // 1..2: P2 runs (remaining 3)
    // 2: P3 arrives (burst 1 < 3) -> preempts P2
    // 2..3: P3 runs to completion (3)
    // 3..6: P2 resumes (remaining 3) -> finishes at 6
    // 6..15: P1 resumes (remaining 9) -> finishes at 15
    const std::vector<Process> workload = {
        Process("P1", 0, 10),
        Process("P2", 1, 4),
        Process("P3", 2, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 5u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 1, 2));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 2, 3));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 3, 6));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 6, 15));
}

TEST(SrtfSchedulerTest, ProcessesArrivingAtDifferentTimes) {
    // P1(0, 3), P2(2, 6), P3(4, 2)
    // 0..2: P1 runs (remaining 1)
    // 2: P2 arrives (burst 6). P1 remaining is 1. 6 < 1 is false -> P1 continues.
    // 2..3: P1 finishes at 3.
    // 3..4: P2 starts (burst 6). At 4, P2 remaining is 5.
    // 4: P3 arrives (burst 2 < 5) -> preempts P2!
    // 4..6: P3 runs to completion at 6.
    // 6..11: P2 resumes (remaining 5) to 11.
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 2, 6),
        Process("P3", 4, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 3, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 6, 11));
}

TEST(SrtfSchedulerTest, AllProcessesArrivingAtTimeZero) {
    // At time 0, identical arrival time behaves like non-preemptive SJF (shortest first)
    const std::vector<Process> workload = {
        Process("P1", 0, 8),
        Process("P2", 0, 3),
        Process("P3", 0, 5),
        Process("P4", 0, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P4", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 1, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 4, 9));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 9, 17));
}

// ============================================================================
// Category B: Preemption Correctness
// ============================================================================

TEST(SrtfSchedulerTest, ShortProcessInterruptsLongProcess) {
    const std::vector<Process> workload = {
        Process("P_Long", 0, 20),
        Process("P_Short", 5, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P_Long", 0, 5));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P_Short", 5, 7));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P_Long", 7, 22));
}

TEST(SrtfSchedulerTest, MultipleArrivalsRepeatedlyPreemptRunningProcess) {
    const std::vector<Process> workload = {
        Process("P1", 0, 20),
        Process("P2", 2, 8),
        Process("P3", 4, 3),
        Process("P4", 5, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // 0..2: P1 (rem 18)
    // 2..4: P2 (rem 6)
    // 4..5: P3 (rem 2)
    // 5..6: P4 (burst 1) completes
    // 6..8: P3 completes (rem 2)
    // 8..14: P2 completes (rem 6)
    // 14..32: P1 completes (rem 18)
    ASSERT_EQ(res.timeline().size(), 7u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 4, 5));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P4", 5, 6));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P3", 6, 8));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P2", 8, 14));
    EXPECT_EQ(res.timeline()[6], ExecutionSegment::process("P1", 14, 32));
}

TEST(SrtfSchedulerTest, ProcessResumesWithExactRemainingExecutionTime) {
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // P1 runs 0..2 (duration 2). P2 runs 2..3. P1 resumes 3..8 (duration 5).
    // Total execution for P1 = 2 + 5 = 7.
    ASSERT_EQ(res.timeline().size(), 3u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 3));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 3, 8));
}

TEST(SrtfSchedulerTest, ProcessPreemptedMoreThanOnce) {
    const std::vector<Process> workload = {
        Process("P1", 0, 15),
        Process("P2", 2, 2),
        Process("P3", 6, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // 0..2: P1 runs (rem 13)
    // 2..4: P2 runs (burst 2) to completion
    // 4..6: P1 resumes (runs 2 units, rem 11)
    // 6: P3 arrives with burst 1 < 11 -> preempts P1 again!
    // 6..7: P3 runs to completion
    // 7..18: P1 resumes (rem 11) to completion
    ASSERT_EQ(res.timeline().size(), 5u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P3", 6, 7));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 7, 18));
}

TEST(SrtfSchedulerTest, ProcessFinishesWithoutInterruption) {
    // P2 arrives with longer burst; P1 executes completely without interruption
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 1, 10)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 4));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 4, 14));
}

TEST(SrtfSchedulerTest, NoUnnecessaryPreemptionWhenRemainingTimesEqual) {
    // P1 arrives at 0 with burst 6.
    // At time 2, P1 has 4 units remaining.
    // P2 arrives at 2 with burst 4.
    // Equal remaining times do NOT trigger preemption!
    // P1 continues until 6. Then P2 runs 6..10.
    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 2, 4)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 6));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 6, 10));
}

TEST(SrtfSchedulerTest, NewlyArrivingProcessLongerRemainingTimeDoesNotPreempt) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 1, 10)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 5, 15));
}

TEST(SrtfSchedulerTest, NewlyArrivingProcessEqualRemainingTimeDoesNotPreempt) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 2, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 4));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 4, 6));
}

// ============================================================================
// Category C: Tie-Breaking
// ============================================================================

TEST(SrtfSchedulerTest, EqualRemainingTimesAtInitialScheduling) {
    // P1 and P2 both arrive at 0 with burst 3. P1 is earlier in input sequence.
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 0, 3)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 3, 6));
}

TEST(SrtfSchedulerTest, EqualRemainingTimesWithDifferentArrivalTimes) {
    // Ready queue contains P1 (rem 3, arr 1) and P2 (rem 3, arr 2).
    // Earlier arrival time (P1) wins.
    const std::vector<Process> workload = {
        Process("P0", 0, 5),
        Process("P1", 1, 3),
        Process("P2", 2, 3)
    };

    // At 1, P0 has 4 rem. P1 has 3 rem < 4 -> P1 preempts P0!
    // P1 runs 1..4. At 2, P2 arrived (burst 3).
    // Equal remaining time (3 vs 3) did not preempt P1.
    // At 4, P1 completes. Ready queue has P2 (rem 3, arr 2) and P0 (rem 4, arr 0).
    // P2 has shorter remaining time (3 < 4), so P2 runs 4..7.
    // Then P0 finishes 7..11.
    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P0", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 1, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 4, 7));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P0", 7, 11));
}

TEST(SrtfSchedulerTest, EqualRemainingTimesWithIdenticalArrivalTimes) {
    // P2 and P3 arrive at 2 with burst 2. P1 runs 0..2 (rem 6-2=4).
    // At 2, P2 and P3 tie in remaining time and arrival time.
    // Original input index decides: P2 before P3.
    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 2, 2),
        Process("P3", 2, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 6, 10));
}

TEST(SrtfSchedulerTest, OriginalInputOrderPreservedWhenArrivalAndRemainingTimesTie) {
    const std::vector<Process> workload = {
        Process("A", 0, 4),
        Process("B", 0, 4),
        Process("C", 0, 4)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("A", 0, 4));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("B", 4, 8));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("C", 8, 12));
}

TEST(SrtfSchedulerTest, RunningProcessRetainsCpuWhenCompetitorHasEqualRemainingTime) {
    // P1 arrives at 0 with burst 5. At time 3, P1 has 2 remaining.
    // P2 arrives at 3 with burst 2.
    // P1 retains CPU!
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 3, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 5, 7));
}

// ============================================================================
// Category D: Arrival and Completion Boundaries
// ============================================================================

TEST(SrtfSchedulerTest, ProcessArrivesExactlyWhenAnotherCompletes) {
    // P1 finishes at 4. P2 arrives at 4.
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 4, 3)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 4));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 4, 7));
    EXPECT_EQ(res.cpu_idle_time(), 0);
}

TEST(SrtfSchedulerTest, ProcessArrivesOneTimeUnitBeforeCompletion) {
    // P1 arrives 0, burst 4 (completes at 4).
    // P2 arrives at 3 with burst 2.
    // At time 3, P1 remaining is 1. P2 burst is 2.
    // 2 < 1 is false -> P1 continues to 4. P2 runs 4..6.
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 3, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 4));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 4, 6));
}

TEST(SrtfSchedulerTest, MultipleProcessesArriveSimultaneously) {
    const std::vector<Process> workload = {
        Process("P1", 2, 5),
        Process("P2", 2, 2),
        Process("P3", 2, 8)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::idle(0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 9));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P3", 9, 17));
}

TEST(SrtfSchedulerTest, ProcessArrivesAtSameTimeAsCompletion) {
    // P1 runs 0..5. At 5, P1 finishes and P2 arrives.
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 5, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 5, 7));
}

TEST(SrtfSchedulerTest, ZeroGapHandoffBetweenProcesses) {
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 1, 2)
    };

    // 0..1: P1 (rem 2)
    // 1: P2 arrives (burst 2). Rem equal -> P1 continues!
    // 0..3: P1 finishes.
    // 3..5: P2 finishes.
    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 3, 5));
}

TEST(SrtfSchedulerTest, MultipleArrivalsDuringLongExecutionInterval) {
    // P1 has burst 20. P2 arrives at 3 (burst 18), P3 arrives at 6 (burst 15).
    // At 3, P1 rem is 17 < 18 -> P1 continues.
    // At 6, P1 rem is 14 < 15 -> P1 continues.
    // P1 finishes 0..20!
    // Then P3 (burst 15) runs 20..35.
    // Then P2 (burst 18) runs 35..53.
    const std::vector<Process> workload = {
        Process("P1", 0, 20),
        Process("P2", 3, 18),
        Process("P3", 6, 15)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 20));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P3", 20, 35));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 35, 53));
}

// ============================================================================
// Category E: CPU Idle Handling
// ============================================================================

TEST(SrtfSchedulerTest, InitialCpuIdlePeriod) {
    const std::vector<Process> workload = {
        Process("P1", 4, 3)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::idle(0, 4));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 4, 7));
    EXPECT_EQ(res.cpu_idle_time(), 4);
    EXPECT_EQ(res.cpu_busy_time(), 3);
    EXPECT_EQ(res.total_execution_time(), 7);
}

TEST(SrtfSchedulerTest, IdlePeriodBetweenCompletedWorkAndNextArrival) {
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 7, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::idle(3, 7));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 7, 9));
    EXPECT_EQ(res.cpu_idle_time(), 4);
}

TEST(SrtfSchedulerTest, MultipleIdlePeriods) {
    const std::vector<Process> workload = {
        Process("P1", 2, 2),
        Process("P2", 7, 3),
        Process("P3", 15, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 6u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::idle(0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::idle(4, 7));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 7, 10));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::idle(10, 15));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P3", 15, 16));
    EXPECT_EQ(res.cpu_idle_time(), 2 + 3 + 5);
}

TEST(SrtfSchedulerTest, MultipleProcessesArrivingAfterIdlePeriod) {
    const std::vector<Process> workload = {
        Process("P1", 5, 4),
        Process("P2", 5, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::idle(0, 5));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 5, 7));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 7, 11));
}

TEST(SrtfSchedulerTest, NoUnnecessaryZeroLengthIdleSegments) {
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 3, 4)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    for (const auto& seg : res.timeline()) {
        EXPECT_GT(seg.duration(), 0);
        EXPECT_NE(seg.start_time, seg.end_time);
    }
}

// ============================================================================
// Category F: Metrics
// ============================================================================

TEST(SrtfSchedulerTest, CorrectCompletionTimes) {
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1)
    };
    // 0..2: P1 (rem 5)
    // 2..4: P2 (rem 2)
    // 4..5: P3 finishes (CT = 5)
    // 5..7: P2 finishes (CT = 7)
    // 7..12: P1 finishes (CT = 12)

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_EQ(res.find_process_state("P3")->completion_time, 5);
    EXPECT_EQ(res.find_process_state("P2")->completion_time, 7);
    EXPECT_EQ(res.find_process_state("P1")->completion_time, 12);
}

TEST(SrtfSchedulerTest, CorrectTurnaroundTimes) {
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // TAT = CT - AT
    EXPECT_EQ(res.find_process_state("P1")->turnaround_time, 12 - 0);
    EXPECT_EQ(res.find_process_state("P2")->turnaround_time, 7 - 2);
    EXPECT_EQ(res.find_process_state("P3")->turnaround_time, 5 - 4);
}

TEST(SrtfSchedulerTest, CorrectWaitingTimes) {
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // WT = TAT - BT
    EXPECT_EQ(res.find_process_state("P1")->waiting_time, 12 - 7); // 5
    EXPECT_EQ(res.find_process_state("P2")->waiting_time, 5 - 4);  // 1
    EXPECT_EQ(res.find_process_state("P3")->waiting_time, 1 - 1);  // 0
}

TEST(SrtfSchedulerTest, CorrectResponseTimes) {
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // RT = First Start - AT
    EXPECT_EQ(res.find_process_state("P1")->response_time, 0 - 0);
    EXPECT_EQ(res.find_process_state("P2")->response_time, 2 - 2);
    EXPECT_EQ(res.find_process_state("P3")->response_time, 4 - 4);
}

TEST(SrtfSchedulerTest, ResponseTimeRemainsBasedOnFirstExecutionAfterPreemption) {
    const std::vector<Process> workload = {
        Process("P1", 0, 10),
        Process("P2", 1, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->start_time, 0);
    EXPECT_EQ(p1->response_time, 0); // 0 - 0 = 0 even though resumed at 3
}

TEST(SrtfSchedulerTest, AverageMetricsAreCorrect) {
    const std::vector<Process> workload = {
        Process("P1", 0, 8),
        Process("P2", 1, 4),
        Process("P3", 2, 2)
    };
    // From Scenario A:
    // P1: CT 14, TAT 14, WT 6, RT 0
    // P2: CT 7, TAT 6, WT 2, RT 0
    // P3: CT 4, TAT 2, WT 0, RT 0
    // Sum TAT = 22, Avg TAT = 22.0 / 3.0
    // Sum WT = 8, Avg WT = 8.0 / 3.0
    // Sum RT = 0, Avg RT = 0.0

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_NEAR(res.average_turnaround_time(), 22.0 / 3.0, 1e-6);
    EXPECT_NEAR(res.average_waiting_time(), 8.0 / 3.0, 1e-6);
    EXPECT_NEAR(res.average_response_time(), 0.0, 1e-6);
}

TEST(SrtfSchedulerTest, BusyTimeCorrectDespiteMultipleSegments) {
    const std::vector<Process> workload = {
        Process("P1", 0, 10),
        Process("P2", 2, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_EQ(res.cpu_busy_time(), 12);
}

TEST(SrtfSchedulerTest, IdleTimeAndUtilizationAreCorrect) {
    const std::vector<Process> workload = {
        Process("P1", 2, 3),
        Process("P2", 7, 5)
    };
    // 0..2 idle (2), 2..5 P1 (3), 5..7 idle (2), 7..12 P2 (5)
    // Total = 12, Busy = 8, Idle = 4, Util = (8 / 12) * 100%

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_EQ(res.total_execution_time(), 12);
    EXPECT_EQ(res.cpu_busy_time(), 8);
    EXPECT_EQ(res.cpu_idle_time(), 4);
    EXPECT_NEAR(res.cpu_utilization(), (8.0 / 12.0) * 100.0, 1e-6);
}

// ============================================================================
// Category G: Boundary and Error Cases
// ============================================================================

TEST(SrtfSchedulerTest, EmptyWorkloadRejected) {
    const std::vector<Process> workload = {};
    const auto outcome = SrtfScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(SrtfSchedulerTest, InvalidProcessIdentifiersRejected) {
    const std::vector<Process> workload = {
        Process("   ", 0, 5)
    };
    const auto outcome = SrtfScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(SrtfSchedulerTest, DuplicateIdentifiersRejected) {
    const std::vector<Process> workload = {
        Process("p1", 0, 5),
        Process("P1", 2, 3)
    };
    const auto outcome = SrtfScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(SrtfSchedulerTest, NegativeArrivalTimeRejected) {
    const std::vector<Process> workload = {
        Process("P1", -1, 5)
    };
    const auto outcome = SrtfScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(SrtfSchedulerTest, ZeroBurstTimeRejected) {
    const std::vector<Process> workload = {
        Process("P1", 0, 0)
    };
    const auto outcome = SrtfScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(SrtfSchedulerTest, NegativeBurstTimeRejected) {
    const std::vector<Process> workload = {
        Process("P1", 0, -5)
    };
    const auto outcome = SrtfScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(SrtfSchedulerTest, LargeSimulationTimes) {
    constexpr Time large_arrival = 1'000'000'000'000LL;
    constexpr Time large_burst = 500'000'000LL;

    const std::vector<Process> workload = {
        Process("P1", large_arrival, large_burst)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_EQ(res.total_execution_time(), large_arrival + large_burst);
    EXPECT_EQ(res.cpu_idle_time(), large_arrival);
    EXPECT_EQ(res.cpu_busy_time(), large_burst);
}

TEST(SrtfSchedulerTest, NearInt64Overflow) {
    constexpr Time near_max = std::numeric_limits<Time>::max() - 1000;
    const std::vector<Process> workload = {
        Process("P1", near_max, 500)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().total_execution_time(), near_max + 500);
}

TEST(SrtfSchedulerTest, ArithmeticOverflowReturnsExpectedError) {
    constexpr Time max_time = std::numeric_limits<Time>::max();
    const std::vector<Process> workload = {
        Process("P1", max_time - 10, 20)
    };

    const auto outcome = SrtfScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_overflow_error());
}

TEST(SrtfSchedulerTest, NoDivisionByZeroErrors) {
    // Single process with zero arrival and 1 burst
    const std::vector<Process> workload = {
        Process("P1", 0, 1)
    };
    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_GT(outcome.value().cpu_utilization(), 0.0);
}

// ============================================================================
// Category H: Result Integrity and Gantt Chart
// ============================================================================

TEST(SrtfSchedulerTest, EveryProcessCompletesExactlyOnce) {
    const std::vector<Process> workload = {
        Process("P1", 0, 8),
        Process("P2", 1, 4),
        Process("P3", 2, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    for (const auto& state : res.processes()) {
        EXPECT_EQ(state.status, ProcessExecutionStatus::Completed);
        EXPECT_EQ(state.remaining_time, 0);
        EXPECT_GT(state.completion_time, 0);
    }
}

TEST(SrtfSchedulerTest, SumOfExecutionSegmentsEqualsBurstTime) {
    const std::vector<Process> workload = {
        Process("P1", 0, 8),
        Process("P2", 1, 4),
        Process("P3", 2, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
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

TEST(SrtfSchedulerTest, ExecutionSegmentsChronologicallyOrdered) {
    const std::vector<Process> workload = {
        Process("P1", 0, 10),
        Process("P2", 2, 3),
        Process("P3", 3, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    for (std::size_t i = 1; i < res.timeline().size(); ++i) {
        EXPECT_EQ(res.timeline()[i - 1].end_time, res.timeline()[i].start_time);
    }
}

TEST(SrtfSchedulerTest, ExecutionSegmentsDoNotOverlap) {
    const std::vector<Process> workload = {
        Process("P1", 0, 8),
        Process("P2", 1, 4),
        Process("P3", 2, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    for (std::size_t i = 1; i < res.timeline().size(); ++i) {
        EXPECT_GE(res.timeline()[i].start_time, res.timeline()[i - 1].end_time);
    }
}

TEST(SrtfSchedulerTest, NoZeroDurationSegments) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 1, 2),
        Process("P3", 3, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    for (const auto& seg : res.timeline()) {
        EXPECT_GT(seg.duration(), 0);
    }
}

TEST(SrtfSchedulerTest, GanttChartCorrectlyDisplaysPreemptedProcesses) {
    const std::vector<Process> workload = {
        Process("P1", 0, 8),
        Process("P2", 1, 4),
        Process("P3", 2, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto chart = GanttChart::render(outcome.value());

    EXPECT_FALSE(chart.empty());
    EXPECT_NE(chart.find("P1"), std::string::npos);
    EXPECT_NE(chart.find("P2"), std::string::npos);
    EXPECT_NE(chart.find("P3"), std::string::npos);
}

TEST(SrtfSchedulerTest, GanttChartCorrectlyDisplaysIdleIntervals) {
    const std::vector<Process> workload = {
        Process("P1", 3, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto chart = GanttChart::render(outcome.value());

    EXPECT_NE(chart.find("Idle"), std::string::npos);
}

TEST(SrtfSchedulerTest, FindProcessStateReturnsExpectedState) {
    const std::vector<Process> workload = {
        Process("ProcessA", 0, 5)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto* state = outcome.value().find_process_state("ProcessA");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->process_id, "ProcessA");
    EXPECT_EQ(state->burst_time, 5);
}

TEST(SrtfSchedulerTest, FindProcessStateReturnsNullptrForUnknownId) {
    const std::vector<Process> workload = {
        Process("ProcessA", 0, 5)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().find_process_state("NonExistent"), nullptr);
}

// ============================================================================
// Mandatory Hand-Calculated Scenarios (Section 8)
// ============================================================================

TEST(SrtfSchedulerTest, ScenarioA_BasicPreemption) {
    const std::vector<Process> workload = {
        Process("P1", 0, 8),
        Process("P2", 1, 4),
        Process("P3", 2, 2)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // Expected timeline:
    // P1: 0–1
    // P2: 1–2
    // P3: 2–4
    // P2: 4–7
    // P1: 7–14
    ASSERT_EQ(res.timeline().size(), 5u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 1, 2));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 2, 4));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 4, 7));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 7, 14));

    // Expected metrics:
    // P1: CT 14, TAT 14, WT 6, RT 0
    // P2: CT 7, TAT 6, WT 2, RT 0
    // P3: CT 4, TAT 2, WT 0, RT 0
    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 14);
    EXPECT_EQ(p1->turnaround_time, 14);
    EXPECT_EQ(p1->waiting_time, 6);
    EXPECT_EQ(p1->response_time, 0);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 7);
    EXPECT_EQ(p2->turnaround_time, 6);
    EXPECT_EQ(p2->waiting_time, 2);
    EXPECT_EQ(p2->response_time, 0);

    const auto* p3 = res.find_process_state("P3");
    ASSERT_NE(p3, nullptr);
    EXPECT_EQ(p3->completion_time, 4);
    EXPECT_EQ(p3->turnaround_time, 2);
    EXPECT_EQ(p3->waiting_time, 0);
    EXPECT_EQ(p3->response_time, 0);
}

TEST(SrtfSchedulerTest, ScenarioB_MultiplePreemptions) {
    const std::vector<Process> workload = {
        Process("P1", 0, 10),
        Process("P2", 2, 5),
        Process("P3", 3, 2),
        Process("P4", 5, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // Expected timeline:
    // P1: 0–2
    // P2: 2–3
    // P3: 3–5
    // P4: 5–6
    // P2: 6–10
    // P1: 10–18
    ASSERT_EQ(res.timeline().size(), 6u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 3));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 3, 5));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P4", 5, 6));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P2", 6, 10));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P1", 10, 18));

    // Expected metrics:
    // P1: CT 18, TAT 18, WT 8, RT 0
    // P2: CT 10, TAT 8, WT 3, RT 0
    // P3: CT 5, TAT 2, WT 0, RT 0
    // P4: CT 6, TAT 1, WT 0, RT 0
    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 18);
    EXPECT_EQ(p1->turnaround_time, 18);
    EXPECT_EQ(p1->waiting_time, 8);
    EXPECT_EQ(p1->response_time, 0);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 10);
    EXPECT_EQ(p2->turnaround_time, 8);
    EXPECT_EQ(p2->waiting_time, 3);
    EXPECT_EQ(p2->response_time, 0);

    const auto* p3 = res.find_process_state("P3");
    ASSERT_NE(p3, nullptr);
    EXPECT_EQ(p3->completion_time, 5);
    EXPECT_EQ(p3->turnaround_time, 2);
    EXPECT_EQ(p3->waiting_time, 0);
    EXPECT_EQ(p3->response_time, 0);

    const auto* p4 = res.find_process_state("P4");
    ASSERT_NE(p4, nullptr);
    EXPECT_EQ(p4->completion_time, 6);
    EXPECT_EQ(p4->turnaround_time, 1);
    EXPECT_EQ(p4->waiting_time, 0);
    EXPECT_EQ(p4->response_time, 0);
}

TEST(SrtfSchedulerTest, ScenarioC_EqualRemainingTimeNoPreemptionRule) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 3)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // Expected timeline:
    // P1: 0–5
    // P2: 5–8
    ASSERT_EQ(res.timeline().size(), 2u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 5, 8));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 5);
    EXPECT_EQ(p1->turnaround_time, 5);
    EXPECT_EQ(p1->waiting_time, 0);
    EXPECT_EQ(p1->response_time, 0);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 8);
    EXPECT_EQ(p2->turnaround_time, 6);
    EXPECT_EQ(p2->waiting_time, 3);
    EXPECT_EQ(p2->response_time, 3);
}

TEST(SrtfSchedulerTest, ScenarioD_CpuIdlePeriods_P1Burst2) {
    // When P1 has burst 2:
    // P1(3, 2), P2(5, 2), P3(15, 1)
    // Timeline matches Section 8 expected timeline exactly:
    // Idle: 0–3, P1: 3–5, P2: 5–7, Idle: 7–15, P3: 15–16
    const std::vector<Process> workload = {
        Process("P1", 3, 2),
        Process("P2", 5, 2),
        Process("P3", 15, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 5u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::idle(0, 3));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 3, 5));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 5, 7));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::idle(7, 15));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P3", 15, 16));

    EXPECT_EQ(res.cpu_idle_time(), 11);
    EXPECT_EQ(res.cpu_busy_time(), 5);
    EXPECT_EQ(res.total_execution_time(), 16);
}

TEST(SrtfSchedulerTest, ScenarioD_CpuIdlePeriods_P1Burst5) {
    // When P1 has burst 5 as written in table:
    // P1(3, 5), P2(5, 2), P3(15, 1)
    // 0..3: Idle
    // 3..5: P1 runs (rem 3)
    // 5: P2 arrives (burst 2 < 3) -> preempts P1!
    // 5..7: P2 runs to completion
    // 7..10: P1 resumes (rem 3) to completion
    // 10..15: Idle
    // 15..16: P3 runs to completion
    const std::vector<Process> workload = {
        Process("P1", 3, 5),
        Process("P2", 5, 2),
        Process("P3", 15, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 6u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::idle(0, 3));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 3, 5));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 5, 7));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 7, 10));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::idle(10, 15));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P3", 15, 16));

    EXPECT_EQ(res.cpu_idle_time(), 8);
    EXPECT_EQ(res.cpu_busy_time(), 8);
    EXPECT_EQ(res.total_execution_time(), 16);
}

TEST(SrtfSchedulerTest, ScenarioE_SameTimeArrivalsAndDeterministicTieBreaking) {
    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 2, 2),
        Process("P3", 2, 2),
        Process("P4", 2, 1)
    };

    const auto outcome = SrtfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // Expected timeline:
    // P1: 0–2
    // P4: 2–3
    // P2: 3–5
    // P3: 5–7
    // P1: 7–11
    ASSERT_EQ(res.timeline().size(), 5u);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P4", 2, 3));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 3, 5));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P3", 5, 7));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 7, 11));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 11);
    EXPECT_EQ(p1->turnaround_time, 11);
    EXPECT_EQ(p1->waiting_time, 5);
    EXPECT_EQ(p1->response_time, 0);

    const auto* p4 = res.find_process_state("P4");
    ASSERT_NE(p4, nullptr);
    EXPECT_EQ(p4->completion_time, 3);
    EXPECT_EQ(p4->turnaround_time, 1);
    EXPECT_EQ(p4->waiting_time, 0);
    EXPECT_EQ(p4->response_time, 0);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 5);
    EXPECT_EQ(p2->turnaround_time, 3);
    EXPECT_EQ(p2->waiting_time, 1);
    EXPECT_EQ(p2->response_time, 1);

    const auto* p3 = res.find_process_state("P3");
    ASSERT_NE(p3, nullptr);
    EXPECT_EQ(p3->completion_time, 7);
    EXPECT_EQ(p3->turnaround_time, 5);
    EXPECT_EQ(p3->waiting_time, 3);
    EXPECT_EQ(p3->response_time, 3);
}
