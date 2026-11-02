#include "cpu_scheduler/gantt_chart.hpp"
#include "cpu_scheduler/sjf_scheduler.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <vector>

namespace cpu_scheduler::testing {

// ============================================================================
// Group A: Basic SJF Scheduling (Tests 1 to 8)
// ============================================================================

TEST(SjfSchedulerTest, SingleProcess) {
    const std::vector<Process> workload = {
        Process("P1", 0, 6, 1)
    };

    const auto outcome = SjfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& result = outcome.value();

    ASSERT_EQ(result.timeline().size(), 1);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P1", 0, 6));

    const auto* p1 = result.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 6);
    EXPECT_EQ(p1->turnaround_time, 6);
    EXPECT_EQ(p1->waiting_time, 0);
    EXPECT_EQ(p1->response_time, 0);
}

TEST(SjfSchedulerTest, MultipleProcessesArrivingAtTimeZero) {
    // All arrive at t=0; should execute in strict ascending burst time order: P3 (2), P1 (5), P2 (8)
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 0, 8),
        Process("P3", 0, 2)
    };

    const auto outcome = SjfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& result = outcome.value();

    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P3", 0, 2));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P1", 2, 7));
    EXPECT_EQ(result.timeline()[2], ExecutionSegment::process("P2", 7, 15));
}

TEST(SjfSchedulerTest, ProcessesWithDifferentBurstTimes) {
    const std::vector<Process> workload = {
        Process("P1", 0, 10),
        Process("P2", 0, 4),
        Process("P3", 0, 1)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0].process_id, "P3");
    EXPECT_EQ(result.timeline()[1].process_id, "P2");
    EXPECT_EQ(result.timeline()[2].process_id, "P1");
}

TEST(SjfSchedulerTest, ProcessesArrivingAtDifferentTimes) {
    // Standard specification example from prompt:
    // P1: arr 0, burst 7
    // P2: arr 2, burst 4
    // P3: arr 4, burst 1
    // P4: arr 5, burst 4
    // Expected order: P1 (0..7), P3 (7..8), P2 (8..12), P4 (12..16)
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1),
        Process("P4", 5, 4)
    };

    const auto outcome = SjfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& result = outcome.value();

    ASSERT_EQ(result.timeline().size(), 4);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P1", 0, 7));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P3", 7, 8));
    EXPECT_EQ(result.timeline()[2], ExecutionSegment::process("P2", 8, 12));
    EXPECT_EQ(result.timeline()[3], ExecutionSegment::process("P4", 12, 16));
}

TEST(SjfSchedulerTest, UnsortedInputWorkload) {
    // Input supplied in arbitrary arrival order
    const std::vector<Process> workload = {
        Process("P4", 5, 4),
        Process("P1", 0, 7),
        Process("P3", 4, 1),
        Process("P2", 2, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 4);
    EXPECT_EQ(result.timeline()[0].process_id, "P1");
    EXPECT_EQ(result.timeline()[1].process_id, "P3");
    EXPECT_EQ(result.timeline()[2].process_id, "P2");
    EXPECT_EQ(result.timeline()[3].process_id, "P4");
}

TEST(SjfSchedulerTest, ShortestAvailableProcessSelectedCorrectly) {
    // At t=0 only P1 is ready. At t=3, P2 (burst 5) and P3 (burst 2) are ready -> P3 chosen
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 1, 5),
        Process("P3", 2, 2)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P3", 3, 5));
    EXPECT_EQ(result.timeline()[2], ExecutionSegment::process("P2", 5, 10));
}

TEST(SjfSchedulerTest, NonPreemptiveExecution) {
    // P1 runs 0..6. Even though P2 arrives at t=1 with burst 1, P1 is NOT preempted
    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 1, 1)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 2);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P1", 0, 6));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P2", 6, 7));
}

TEST(SjfSchedulerTest, FutureShortProcessCannotInterruptCurrentExecution) {
    // Edge case from Section 4:
    // P1: arr 0, burst 8
    // P2: arr 1, burst 2
    // P3: arr 3, burst 1
    // Expected: P1 (0..8) -> P3 (8..9) -> P2 (9..11)
    const std::vector<Process> workload = {
        Process("P1", 0, 8),
        Process("P2", 1, 2),
        Process("P3", 3, 1)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P1", 0, 8));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P3", 8, 9));
    EXPECT_EQ(result.timeline()[2], ExecutionSegment::process("P2", 9, 11));
}

// ============================================================================
// Group B: Tie-Breaking (Tests 9 to 13)
// ============================================================================

TEST(SjfSchedulerTest, EqualBurstTimes) {
    // Both arrive at 0 with burst 4. Tie broken by original input order: P1 then P2
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 0, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 2);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P1", 0, 4));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P2", 4, 8));
}

TEST(SjfSchedulerTest, EqualBurstTimesWithDifferentArrivalTimes) {
    // Both ready at t=5. Burst 3 == 3. P2 arrived at 2, P3 arrived at 4 -> P2 chosen first
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 3),
        Process("P3", 4, 3)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0].process_id, "P1");
    EXPECT_EQ(result.timeline()[1].process_id, "P2");
    EXPECT_EQ(result.timeline()[2].process_id, "P3");
}

TEST(SjfSchedulerTest, EqualBurstAndArrivalTimes) {
    // Both arrive at t=5, both burst 3. Original order ProcA, ProcB preserved
    const std::vector<Process> workload = {
        Process("ProcA", 5, 3),
        Process("ProcB", 5, 3)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 3); // Idle 0..5, then ProcA, ProcB
    EXPECT_TRUE(result.timeline()[0].is_idle());
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("ProcA", 5, 8));
    EXPECT_EQ(result.timeline()[2], ExecutionSegment::process("ProcB", 8, 11));
}

TEST(SjfSchedulerTest, StableOriginalInputOrderPreservation) {
    // Three simultaneous identical processes at time 0
    const std::vector<Process> workload = {
        Process("First", 0, 5),
        Process("Second", 0, 5),
        Process("Third", 0, 5)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0].process_id, "First");
    EXPECT_EQ(result.timeline()[1].process_id, "Second");
    EXPECT_EQ(result.timeline()[2].process_id, "Third");
}

TEST(SjfSchedulerTest, MultipleGroupsOfTiedProcesses) {
    // Group 1 (burst 2): P1 (arr 1), P2 (arr 0) -> P2 first, then P1
    // Group 2 (burst 5): P3 (arr 2), P4 (arr 2) -> P3 first (idx 2), then P4 (idx 3)
    const std::vector<Process> workload = {
        Process("P1", 1, 2),
        Process("P2", 0, 2),
        Process("P3", 2, 5),
        Process("P4", 2, 5)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 4);
    EXPECT_EQ(result.timeline()[0].process_id, "P2"); // burst 2, arr 0
    EXPECT_EQ(result.timeline()[1].process_id, "P1"); // burst 2, arr 1
    EXPECT_EQ(result.timeline()[2].process_id, "P3"); // burst 5, arr 2, idx 2
    EXPECT_EQ(result.timeline()[3].process_id, "P4"); // burst 5, arr 2, idx 3
}

// ============================================================================
// Group C: Ready-Queue Correctness (Tests 14 to 18)
// ============================================================================

TEST(SjfSchedulerTest, OnlyArrivedProcessesConsidered) {
    // P3 has burst 1, but arrives at t=10. At t=0 only P1 is available
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 2, 4),
        Process("P3", 10, 1)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 4);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P2", 5, 9));
    EXPECT_EQ(result.timeline()[2], ExecutionSegment::idle(9, 10));
    EXPECT_EQ(result.timeline()[3], ExecutionSegment::process("P3", 10, 11));
}

TEST(SjfSchedulerTest, NewProcessArrivingDuringAnotherProcessExecution) {
    // P1 runs 0..6. During this, P2 arrives at t=2 (burst 4) and P3 arrives at t=3 (burst 2)
    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 2, 4),
        Process("P3", 3, 2)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0].process_id, "P1");
    EXPECT_EQ(result.timeline()[1].process_id, "P3"); // shorter than P2
    EXPECT_EQ(result.timeline()[2].process_id, "P2");
}

TEST(SjfSchedulerTest, MultipleProcessesBecomingAvailableWhileCpuIsBusy) {
    // P1 runs 0..10. While busy, P2 (arr 1, burst 6), P3 (arr 2, burst 2), P4 (arr 3, burst 4) arrive
    const std::vector<Process> workload = {
        Process("P1", 0, 10),
        Process("P2", 1, 6),
        Process("P3", 2, 2),
        Process("P4", 3, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 4);
    EXPECT_EQ(result.timeline()[0].process_id, "P1");
    EXPECT_EQ(result.timeline()[1].process_id, "P3"); // burst 2
    EXPECT_EQ(result.timeline()[2].process_id, "P4"); // burst 4
    EXPECT_EQ(result.timeline()[3].process_id, "P2"); // burst 6
}

TEST(SjfSchedulerTest, CorrectSelectionAfterEveryCompletion) {
    // Dynamic change: P1 runs 0..4. At 4, ready queue has P2 (arr 1, burst 5).
    // P2 runs 4..9. While P2 runs, P3 (arr 6, burst 2) arrives.
    // At t=9, P3 executes immediately
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 1, 5),
        Process("P3", 6, 2)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P1", 0, 4));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P2", 4, 9));
    EXPECT_EQ(result.timeline()[2], ExecutionSegment::process("P3", 9, 11));
}

TEST(SjfSchedulerTest, SelectionChangesDynamicallyAsReadyQueueChanges) {
    // At t=0: P1 (burst 8).
    // At t=2: P2 (burst 6) arrives.
    // At t=5: P3 (burst 2) arrives.
    // At t=8: P1 completes. Queue has P3 (burst 2) and P2 (burst 6). P3 selected!
    const std::vector<Process> workload = {
        Process("P1", 0, 8),
        Process("P2", 2, 6),
        Process("P3", 5, 2)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0].process_id, "P1");
    EXPECT_EQ(result.timeline()[1].process_id, "P3");
    EXPECT_EQ(result.timeline()[2].process_id, "P2");
}

// ============================================================================
// Group D: CPU Idle Handling (Tests 19 to 24)
// ============================================================================

TEST(SjfSchedulerTest, InitialCpuIdlePeriod) {
    // Prompt example: P1 (arr 5, burst 3), P2 (arr 6, burst 1)
    // Idle 0..5 -> P1 (5..8) -> P2 (8..9)
    const std::vector<Process> workload = {
        Process("P1", 5, 3),
        Process("P2", 6, 1)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::idle(0, 5));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P1", 5, 8));
    EXPECT_EQ(result.timeline()[2], ExecutionSegment::process("P2", 8, 9));

    EXPECT_EQ(result.cpu_idle_time(), 5);
    EXPECT_EQ(result.cpu_busy_time(), 4);
    EXPECT_EQ(result.total_execution_time(), 9);
}

TEST(SjfSchedulerTest, IntermediateIdlePeriod) {
    // Prompt example: P1 (arr 0, burst 3), P2 (arr 10, burst 2)
    // P1 (0..3) -> Idle (3..10) -> P2 (10..12)
    const std::vector<Process> workload = {
        Process("P1", 0, 3),
        Process("P2", 10, 2)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::idle(3, 10));
    EXPECT_EQ(result.timeline()[2], ExecutionSegment::process("P2", 10, 12));

    EXPECT_EQ(result.cpu_idle_time(), 7);
}

TEST(SjfSchedulerTest, MultipleIdleIntervals) {
    // Idle 0..2 -> P1 (2..4) -> Idle 4..8 -> P2 (8..10) -> Idle 10..15 -> P3 (15..17)
    const std::vector<Process> workload = {
        Process("P1", 2, 2),
        Process("P2", 8, 2),
        Process("P3", 15, 2)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 6);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::idle(0, 2));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P1", 2, 4));
    EXPECT_EQ(result.timeline()[2], ExecutionSegment::idle(4, 8));
    EXPECT_EQ(result.timeline()[3], ExecutionSegment::process("P2", 8, 10));
    EXPECT_EQ(result.timeline()[4], ExecutionSegment::idle(10, 15));
    EXPECT_EQ(result.timeline()[5], ExecutionSegment::process("P3", 15, 17));

    EXPECT_EQ(result.cpu_idle_time(), 2 + 4 + 5);
    EXPECT_EQ(result.cpu_busy_time(), 6);
    EXPECT_EQ(result.total_execution_time(), 17);
}

TEST(SjfSchedulerTest, SimultaneousArrivalsAfterAnIdlePeriod) {
    // Idle 0..4 -> At t=4, P1 (arr 4, burst 6) and P2 (arr 4, burst 2) arrive
    // P2 has shorter burst -> P2 runs 4..6, then P1 runs 6..12
    const std::vector<Process> workload = {
        Process("P1", 4, 6),
        Process("P2", 4, 2)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 3);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::idle(0, 4));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P2", 4, 6));
    EXPECT_EQ(result.timeline()[2], ExecutionSegment::process("P1", 6, 12));
}

TEST(SjfSchedulerTest, ExactArrivalCompletionBoundary) {
    // P1 finishes at 5, P2 arrives exactly at 5 -> No idle segment!
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 5, 3)
    };

    const auto result = SjfScheduler::run(workload).value();
    ASSERT_EQ(result.timeline().size(), 2);
    EXPECT_EQ(result.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    EXPECT_EQ(result.timeline()[1], ExecutionSegment::process("P2", 5, 8));
    EXPECT_EQ(result.cpu_idle_time(), 0);
}

TEST(SjfSchedulerTest, NoZeroDurationIdleSegments) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 4, 2),
        Process("P3", 6, 3)
    };

    const auto result = SjfScheduler::run(workload).value();
    for (const auto& seg : result.timeline()) {
        EXPECT_GT(seg.duration(), 0);
        EXPECT_FALSE(seg.is_idle());
    }
}

// ============================================================================
// Group E: Metrics (Tests 25 to 34)
// ============================================================================

TEST(SjfSchedulerTest, CorrectCompletionTimes) {
    // Workload from Section 2:
    // P1: arr 0, burst 7 -> CT = 7
    // P3: arr 4, burst 1 -> CT = 8
    // P2: arr 2, burst 4 -> CT = 12
    // P4: arr 5, burst 4 -> CT = 16
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1),
        Process("P4", 5, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    EXPECT_EQ(result.find_process_state("P1")->completion_time, 7);
    EXPECT_EQ(result.find_process_state("P2")->completion_time, 12);
    EXPECT_EQ(result.find_process_state("P3")->completion_time, 8);
    EXPECT_EQ(result.find_process_state("P4")->completion_time, 16);
}

TEST(SjfSchedulerTest, CorrectTurnaroundTimes) {
    // TAT = CT - Arrival
    // P1: 7 - 0 = 7
    // P2: 12 - 2 = 10
    // P3: 8 - 4 = 4
    // P4: 16 - 5 = 11
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1),
        Process("P4", 5, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    EXPECT_EQ(result.find_process_state("P1")->turnaround_time, 7);
    EXPECT_EQ(result.find_process_state("P2")->turnaround_time, 10);
    EXPECT_EQ(result.find_process_state("P3")->turnaround_time, 4);
    EXPECT_EQ(result.find_process_state("P4")->turnaround_time, 11);
}

TEST(SjfSchedulerTest, CorrectWaitingTimes) {
    // WT = TAT - Burst
    // P1: 7 - 7 = 0
    // P2: 10 - 4 = 6
    // P3: 4 - 1 = 3
    // P4: 11 - 4 = 7
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1),
        Process("P4", 5, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    EXPECT_EQ(result.find_process_state("P1")->waiting_time, 0);
    EXPECT_EQ(result.find_process_state("P2")->waiting_time, 6);
    EXPECT_EQ(result.find_process_state("P3")->waiting_time, 3);
    EXPECT_EQ(result.find_process_state("P4")->waiting_time, 7);
}

TEST(SjfSchedulerTest, CorrectResponseTimes) {
    // In non-preemptive SJF, RT must equal WT
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1),
        Process("P4", 5, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    for (const auto& ps : result.processes()) {
        EXPECT_EQ(ps.response_time, ps.waiting_time);
    }
}

TEST(SjfSchedulerTest, AverageWaitingTime) {
    // Sum WT = 0 + 6 + 3 + 7 = 16. Avg WT = 16 / 4 = 4.0
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1),
        Process("P4", 5, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    EXPECT_DOUBLE_EQ(result.average_waiting_time(), 4.0);
}

TEST(SjfSchedulerTest, AverageTurnaroundTime) {
    // Sum TAT = 7 + 10 + 4 + 11 = 32. Avg TAT = 32 / 4 = 8.0
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1),
        Process("P4", 5, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    EXPECT_DOUBLE_EQ(result.average_turnaround_time(), 8.0);
}

TEST(SjfSchedulerTest, AverageResponseTime) {
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1),
        Process("P4", 5, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    EXPECT_DOUBLE_EQ(result.average_response_time(), 4.0);
}

TEST(SjfSchedulerTest, CpuBusyTime) {
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1),
        Process("P4", 5, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    EXPECT_EQ(result.cpu_busy_time(), 7 + 4 + 1 + 4);
}

TEST(SjfSchedulerTest, CpuIdleTime) {
    const std::vector<Process> workload = {
        Process("P1", 5, 3),
        Process("P2", 12, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    // Idle 0..5 (5) and 8..12 (4) -> total 9
    EXPECT_EQ(result.cpu_idle_time(), 9);
}

TEST(SjfSchedulerTest, CpuUtilization) {
    const std::vector<Process> workload = {
        Process("P1", 5, 3),
        Process("P2", 12, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    // Total = 16, Busy = 7 -> 7 / 16 * 100 = 43.75%
    EXPECT_DOUBLE_EQ(result.cpu_utilization(), 43.75);
}

// ============================================================================
// Group F: Invalid and Boundary Inputs (Tests 35 to 42)
// ============================================================================

TEST(SjfSchedulerTest, EmptyWorkloadRejected) {
    const std::vector<Process> workload;
    const auto outcome = SjfScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(SjfSchedulerTest, EmptyProcessIdRejected) {
    const std::vector<Process> workload = {
        Process("", 0, 4)
    };
    const auto outcome = SjfScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(SjfSchedulerTest, DuplicateProcessIdsRejected) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("  p1  ", 2, 3)
    };
    const auto outcome = SjfScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(SjfSchedulerTest, NegativeArrivalTimeRejected) {
    const std::vector<Process> workload = {
        Process("P1", -3, 5)
    };
    const auto outcome = SjfScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(SjfSchedulerTest, ZeroBurstTimeRejected) {
    const std::vector<Process> workload = {
        Process("P1", 0, 0)
    };
    const auto outcome = SjfScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(SjfSchedulerTest, NegativeBurstTimeRejected) {
    const std::vector<Process> workload = {
        Process("P1", 0, -2)
    };
    const auto outcome = SjfScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_validation_error());
}

TEST(SjfSchedulerTest, LargeValidTimeValues) {
    constexpr Time large_arrival = 2'000'000'000'000LL;
    constexpr Time large_burst = 3'000'000'000'000LL;

    const std::vector<Process> workload = {
        Process("P1", large_arrival, large_burst)
    };

    const auto outcome = SjfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& result = outcome.value();

    EXPECT_EQ(result.find_process_state("P1")->completion_time, large_arrival + large_burst);
    EXPECT_EQ(result.cpu_idle_time(), large_arrival);
    EXPECT_EQ(result.cpu_busy_time(), large_burst);
}

TEST(SjfSchedulerTest, ArithmeticOverflowProtection) {
    const Time near_max = std::numeric_limits<Time>::max() - 10;
    const std::vector<Process> workload = {
        Process("P1", near_max, 50)
    };

    const auto outcome = SjfScheduler::run(workload);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(outcome.is_overflow_error());
}

// ============================================================================
// Group G: Result Integrity (Tests 43 to 48)
// ============================================================================

TEST(SjfSchedulerTest, OriginalProcessObjectsRemainUnchanged) {
    const std::vector<Process> workload = {
        Process("P1", 2, 5),
        Process("P2", 0, 3)
    };
    const std::vector<Process> copy = workload;

    const auto outcome = SjfScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(workload, copy);
}

TEST(SjfSchedulerTest, ExecutionSegmentsChronologicallyOrdered) {
    const std::vector<Process> workload = {
        Process("P1", 5, 2),
        Process("P2", 0, 4),
        Process("P3", 12, 1)
    };

    const auto result = SjfScheduler::run(workload).value();
    const auto& timeline = result.timeline();

    ASSERT_GE(timeline.size(), 2);
    for (std::size_t i = 1; i < timeline.size(); ++i) {
        EXPECT_EQ(timeline[i - 1].end_time, timeline[i].start_time);
        EXPECT_LT(timeline[i].start_time, timeline[i].end_time);
    }
}

TEST(SjfSchedulerTest, ExecutionSegmentsDoNotOverlap) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5),
        Process("P2", 1, 2),
        Process("P3", 2, 1)
    };

    const auto result = SjfScheduler::run(workload).value();
    const auto& timeline = result.timeline();

    for (std::size_t i = 1; i < timeline.size(); ++i) {
        EXPECT_LE(timeline[i - 1].end_time, timeline[i].start_time);
    }
}

TEST(SjfSchedulerTest, EveryProcessExecutesExactlyOnce) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4),
        Process("P2", 1, 2),
        Process("P3", 2, 1),
        Process("P4", 10, 3)
    };

    const auto result = SjfScheduler::run(workload).value();

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

TEST(SjfSchedulerTest, TotalBusyTimeEqualsSumOfProcessBurstTimes) {
    const std::vector<Process> workload = {
        Process("P1", 0, 6),
        Process("P2", 2, 3),
        Process("P3", 8, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    Time sum_burst = 0;
    for (const auto& p : workload) {
        sum_burst += p.burst_time;
    }

    EXPECT_EQ(result.cpu_busy_time(), sum_burst);
}

TEST(SjfSchedulerTest, TotalSimulationTimeEqualsBusyPlusIdleTime) {
    const std::vector<Process> workload = {
        Process("P1", 3, 4),
        Process("P2", 15, 2)
    };

    const auto result = SjfScheduler::run(workload).value();
    EXPECT_EQ(result.total_execution_time(), result.cpu_busy_time() + result.cpu_idle_time());
}

// ============================================================================
// Group H: Gantt Chart Integration (Tests 49 to 52)
// ============================================================================

TEST(SjfSchedulerTest, CorrectSjfTimelineRendering) {
    const std::vector<Process> workload = {
        Process("P1", 0, 7),
        Process("P2", 2, 4),
        Process("P3", 4, 1),
        Process("P4", 5, 4)
    };

    const auto result = SjfScheduler::run(workload).value();
    const std::string chart = GanttChart::render(result);

    EXPECT_FALSE(chart.empty());
    EXPECT_NE(chart.find("P1"), std::string::npos);
    EXPECT_NE(chart.find("P3"), std::string::npos);
    EXPECT_NE(chart.find("P2"), std::string::npos);
    EXPECT_NE(chart.find("P4"), std::string::npos);
    EXPECT_NE(chart.find("0"), std::string::npos);
    EXPECT_NE(chart.find("7"), std::string::npos);
    EXPECT_NE(chart.find("8"), std::string::npos);
    EXPECT_NE(chart.find("12"), std::string::npos);
    EXPECT_NE(chart.find("16"), std::string::npos);
}

TEST(SjfSchedulerTest, CorrectRenderingOfIdleIntervals) {
    const std::vector<Process> workload = {
        Process("P1", 2, 3),
        Process("P2", 8, 2)
    };

    const auto result = SjfScheduler::run(workload).value();
    const std::string chart = GanttChart::render(result);

    EXPECT_NE(chart.find("Idle"), std::string::npos);
    EXPECT_NE(chart.find("0"), std::string::npos);
    EXPECT_NE(chart.find("2"), std::string::npos);
    EXPECT_NE(chart.find("5"), std::string::npos);
    EXPECT_NE(chart.find("8"), std::string::npos);
    EXPECT_NE(chart.find("10"), std::string::npos);
}

TEST(SjfSchedulerTest, MultipleDigitProcessIds) {
    const std::vector<Process> workload = {
        Process("Process_100", 0, 4),
        Process("Process_200", 1, 2)
    };

    const auto result = SjfScheduler::run(workload).value();
    const std::string chart = GanttChart::render(result);

    EXPECT_NE(chart.find("Process_100"), std::string::npos);
    EXPECT_NE(chart.find("Process_200"), std::string::npos);
}

TEST(SjfSchedulerTest, EmptyTimelineBehavior) {
    const std::vector<ExecutionSegment> empty_timeline;
    const std::string chart = GanttChart::render(empty_timeline);

    EXPECT_EQ(chart, "[Empty Timeline]\n");
}

} // namespace cpu_scheduler::testing
