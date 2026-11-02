#include "cpu_scheduler/priority_scheduler.hpp"
#include "cpu_scheduler/gantt_chart.hpp"
#include "cpu_scheduler/process.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

namespace cpu_scheduler {
namespace {

constexpr double kEpsilon = 1e-4;

void verify_timeline_invariants(const SchedulingResult& result, const std::vector<Process>& workload) {
    const auto& timeline = result.timeline();
    const auto& states = result.processes();

    // 1. Invariant: Sum of busy segment durations equals total workload burst time
    Time total_workload_burst = 0;
    for (const auto& proc : workload) {
        total_workload_burst += proc.burst_time;
    }

    Time accumulated_busy_time = 0;
    Time accumulated_idle_time = 0;
    Time last_end = 0;

    for (std::size_t i = 0; i < timeline.size(); ++i) {
        const auto& seg = timeline[i];
        EXPECT_LE(seg.start_time, seg.end_time);
        EXPECT_GT(seg.end_time, seg.start_time);

        if (i == 0) {
            EXPECT_EQ(seg.start_time, 0);
        } else {
            EXPECT_EQ(seg.start_time, last_end);
        }

        if (seg.is_idle()) {
            accumulated_idle_time += seg.duration();
        } else {
            accumulated_busy_time += seg.duration();
        }
        last_end = seg.end_time;
    }

    EXPECT_EQ(accumulated_busy_time, total_workload_burst);
    EXPECT_EQ(accumulated_busy_time, result.cpu_busy_time());
    EXPECT_EQ(accumulated_idle_time, result.cpu_idle_time());
    EXPECT_EQ(result.total_execution_time(), accumulated_busy_time + accumulated_idle_time);

    // 2. Invariant: Per-process execution time equals original burst time
    for (const auto& proc : workload) {
        Time proc_executed_time = 0;
        for (const auto& seg : timeline) {
            if (!seg.is_idle() && seg.process_id == proc.id) {
                proc_executed_time += seg.duration();
            }
        }
        EXPECT_EQ(proc_executed_time, proc.burst_time)
            << "Process " << proc.id << " execution duration sum mismatch";
    }

    // 3. Invariant: TAT = CT - AT, WT = TAT - BT, RT = ST - AT
    for (const auto& st : states) {
        EXPECT_EQ(st.status, ProcessExecutionStatus::Completed);
        EXPECT_EQ(st.remaining_time, 0);
        EXPECT_TRUE(st.start_time.has_value());
        EXPECT_GE(st.completion_time, st.arrival_time + st.burst_time);

        EXPECT_EQ(st.turnaround_time, st.completion_time - st.arrival_time);
        EXPECT_EQ(st.waiting_time, st.turnaround_time - st.burst_time);
        EXPECT_GE(st.waiting_time, 0);

        EXPECT_EQ(st.response_time, st.start_time.value() - st.arrival_time);
        EXPECT_GE(st.response_time, 0);
        EXPECT_LE(st.response_time, st.waiting_time);
    }
}

// ============================================================================
// Non-Preemptive Priority Scheduler Tests (32 tests)
// ============================================================================

TEST(NonPreemptivePrioritySchedulerTest, SingleProcess) {
    const std::vector<Process> workload = { Process("P1", 0, 5, 1) };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    EXPECT_EQ(res.total_execution_time(), 5);
    EXPECT_EQ(res.cpu_busy_time(), 5);
    EXPECT_EQ(res.cpu_idle_time(), 0);
    EXPECT_NEAR(res.cpu_utilization(), 100.0, kEpsilon);

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->start_time.value(), 0);
    EXPECT_EQ(p1->completion_time, 5);
    EXPECT_EQ(p1->turnaround_time, 5);
    EXPECT_EQ(p1->waiting_time, 0);
    EXPECT_EQ(p1->response_time, 0);

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, TwoProcessesDifferentPriorities) {
    // Both arrive at 0; P2 has higher priority (1 < 2)
    const std::vector<Process> workload = {
        Process("P1", 0, 5, 2),
        Process("P2", 0, 3, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 2);

    EXPECT_EQ(tl[0].process_id, "P2");
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 3);

    EXPECT_EQ(tl[1].process_id, "P1");
    EXPECT_EQ(tl[1].start_time, 3);
    EXPECT_EQ(tl[1].end_time, 8);

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, SeveralSimultaneousArrivals) {
    // P1(0, 4, prio 3), P2(0, 2, prio 1), P3(0, 6, prio 2)
    // Execution order: P2 (prio 1) -> P3 (prio 2) -> P1 (prio 3)
    const std::vector<Process> workload = {
        Process("P1", 0, 4, 3),
        Process("P2", 0, 2, 1),
        Process("P3", 0, 6, 2)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 3);

    EXPECT_EQ(tl[0].process_id, "P2");
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 2);

    EXPECT_EQ(tl[1].process_id, "P3");
    EXPECT_EQ(tl[1].start_time, 2);
    EXPECT_EQ(tl[1].end_time, 8);

    EXPECT_EQ(tl[2].process_id, "P1");
    EXPECT_EQ(tl[2].start_time, 8);
    EXPECT_EQ(tl[2].end_time, 12);

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, DifferentArrivalTimes) {
    // P1 arrives at 0 with lower priority, runs first because it is the only ready process
    const std::vector<Process> workload = {
        Process("P1", 0, 4, 5),
        Process("P2", 2, 3, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 2);

    EXPECT_EQ(tl[0].process_id, "P1");
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 4);

    EXPECT_EQ(tl[1].process_id, "P2");
    EXPECT_EQ(tl[1].start_time, 4);
    EXPECT_EQ(tl[1].end_time, 7);

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, UnsortedInputWorkloads) {
    // Input provided in reverse arrival order
    const std::vector<Process> workload = {
        Process("P3", 4, 2, 1),
        Process("P2", 2, 3, 2),
        Process("P1", 0, 2, 3)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 3);

    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 2]
    EXPECT_EQ(tl[1].process_id, "P2"); // [2, 5]
    EXPECT_EQ(tl[2].process_id, "P3"); // [5, 7]

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, HigherPriorityArrivesWhileRunningDoesNotInterrupt) {
    // Non-preemptive: P1 starts at 0 with burst 8, P2 arrives at 1 with priority 1.
    // P1 MUST NOT be interrupted.
    const std::vector<Process> workload = {
        Process("P1", 0, 8, 10),
        Process("P2", 1, 2, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 2);

    EXPECT_EQ(tl[0].process_id, "P1");
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 8);

    EXPECT_EQ(tl[1].process_id, "P2");
    EXPECT_EQ(tl[1].start_time, 8);
    EXPECT_EQ(tl[1].end_time, 10);

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, EqualPriorityTieBreakByArrival) {
    // P1 runs [0, 5]. During [0, 5], P2 arrives at 1 and P3 arrives at 2 with equal priority 1.
    // P2 arrived earlier (1 < 2), so P2 must execute before P3.
    const std::vector<Process> workload = {
        Process("P1", 0, 5, 5),
        Process("P3", 2, 2, 1),
        Process("P2", 1, 3, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 3);

    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 5]
    EXPECT_EQ(tl[1].process_id, "P2"); // [5, 8]
    EXPECT_EQ(tl[2].process_id, "P3"); // [8, 10]

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, EqualPriorityTieBreakByOriginalOrder) {
    // Both P2 and P3 arrive at same time with equal priority.
    // P2 is index 1, P3 is index 2. P2 executes first.
    const std::vector<Process> workload = {
        Process("P1", 0, 3, 4),
        Process("P2", 1, 2, 1),
        Process("P3", 1, 2, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 3);

    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 3]
    EXPECT_EQ(tl[1].process_id, "P2"); // [3, 5]
    EXPECT_EQ(tl[2].process_id, "P3"); // [5, 7]

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, NegativePriorityValues) {
    // Negative priorities: -5 (highest) > 0 > 2 (lowest)
    const std::vector<Process> workload = {
        Process("P1", 0, 3, 2),
        Process("P2", 0, 2, -5),
        Process("P3", 0, 4, 0)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 3);

    EXPECT_EQ(tl[0].process_id, "P2"); // [0, 2] (prio -5)
    EXPECT_EQ(tl[1].process_id, "P3"); // [2, 6] (prio 0)
    EXPECT_EQ(tl[2].process_id, "P1"); // [6, 9] (prio 2)

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, ZeroPriority) {
    const std::vector<Process> workload = {
        Process("P1", 0, 2, 0),
        Process("P2", 0, 3, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 2);

    EXPECT_EQ(tl[0].process_id, "P1"); // prio 0 > prio 1
    EXPECT_EQ(tl[1].process_id, "P2");

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, InitialIdleTime) {
    const std::vector<Process> workload = {
        Process("P1", 5, 3, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 2);

    EXPECT_TRUE(tl[0].is_idle());
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 5);

    EXPECT_FALSE(tl[1].is_idle());
    EXPECT_EQ(tl[1].process_id, "P1");
    EXPECT_EQ(tl[1].start_time, 5);
    EXPECT_EQ(tl[1].end_time, 8);

    EXPECT_EQ(res.cpu_idle_time(), 5);
    EXPECT_EQ(res.cpu_busy_time(), 3);
    EXPECT_EQ(res.total_execution_time(), 8);

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, MultipleIdleIntervals) {
    const std::vector<Process> workload = {
        Process("P1", 2, 2, 1),
        Process("P2", 7, 3, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 4);

    EXPECT_TRUE(tl[0].is_idle()); // [0, 2]
    EXPECT_EQ(tl[1].process_id, "P1"); // [2, 4]
    EXPECT_TRUE(tl[2].is_idle()); // [4, 7]
    EXPECT_EQ(tl[3].process_id, "P2"); // [7, 10]

    EXPECT_EQ(res.cpu_idle_time(), 5);
    EXPECT_EQ(res.cpu_busy_time(), 5);
    EXPECT_EQ(res.total_execution_time(), 10);

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, NoFutureProcessSelectedPrematurely) {
    // P2 has higher priority (1 < 2) but arrives in the future at 10.
    // P1 arrives at 0 and must run immediately; it cannot wait for P2.
    const std::vector<Process> workload = {
        Process("P1", 0, 3, 2),
        Process("P2", 10, 2, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 3);

    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 3]
    EXPECT_TRUE(tl[1].is_idle()); // [3, 10]
    EXPECT_EQ(tl[2].process_id, "P2"); // [10, 12]

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, LargePriorityDifferences) {
    const std::vector<Process> workload = {
        Process("P1", 0, 2, 1000000),
        Process("P2", 0, 2, -1000000)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 2);
    EXPECT_EQ(tl[0].process_id, "P2");
    EXPECT_EQ(tl[1].process_id, "P1");

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, LargeArrivalAndBurstValues) {
    constexpr Time large_arr = 1'000'000'000;
    constexpr Time large_burst = 500'000'000;
    const std::vector<Process> workload = {
        Process("P1", large_arr, large_burst, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    EXPECT_EQ(res.total_execution_time(), large_arr + large_burst);
    EXPECT_EQ(res.cpu_busy_time(), large_burst);
    EXPECT_EQ(res.cpu_idle_time(), large_arr);

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, ArithmeticOverflowReturnsError) {
    const std::vector<Process> workload = {
        Process("P1", std::numeric_limits<Time>::max() - 10, 100, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ArithmeticOverflow);
}

TEST(NonPreemptivePrioritySchedulerTest, ValidationEmptyWorkload) {
    const std::vector<Process> workload = {};
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
}

TEST(NonPreemptivePrioritySchedulerTest, ValidationDuplicateId) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5, 1),
        Process("p1", 1, 3, 2)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
}

TEST(NonPreemptivePrioritySchedulerTest, ValidationNegativeArrival) {
    const std::vector<Process> workload = { Process("P1", -1, 5, 1) };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
}

TEST(NonPreemptivePrioritySchedulerTest, ValidationZeroBurst) {
    const std::vector<Process> workload = { Process("P1", 0, 0, 1) };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
}

TEST(NonPreemptivePrioritySchedulerTest, WorkloadImmutability) {
    const std::vector<Process> original = {
        Process("P1", 0, 5, 3),
        Process("P2", 2, 4, 1)
    };
    const std::vector<Process> copy = original;

    const auto outcome = NonPreemptivePriorityScheduler::run(original);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(original, copy);
}

TEST(NonPreemptivePrioritySchedulerTest, TimelineIntegrity) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4, 2),
        Process("P2", 1, 3, 1),
        Process("P3", 2, 1, 3)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(NonPreemptivePrioritySchedulerTest, GanttChartRendering) {
    const std::vector<Process> workload = {
        Process("P1", 0, 3, 2),
        Process("P2", 0, 2, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const std::string chart = GanttChart::render(outcome.value().timeline());
    EXPECT_FALSE(chart.empty());
    EXPECT_NE(chart.find("P2"), std::string::npos);
    EXPECT_NE(chart.find("P1"), std::string::npos);
}

TEST(NonPreemptivePrioritySchedulerTest, MultipleProcessesIdenticalPriority) {
    // All equal priority 5: behaves identically to FCFS
    const std::vector<Process> workload = {
        Process("P1", 0, 4, 5),
        Process("P2", 1, 2, 5),
        Process("P3", 2, 3, 5)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& tl = outcome.value().timeline();
    ASSERT_EQ(tl.size(), 3);
    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 4]
    EXPECT_EQ(tl[1].process_id, "P2"); // [4, 6]
    EXPECT_EQ(tl[2].process_id, "P3"); // [6, 9]

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(NonPreemptivePrioritySchedulerTest, MandatoryExample_Section6_3) {
    // From Batch 6 Section 6.3:
    // P1: arr 0, burst 5, prio 3
    // P2: arr 1, burst 3, prio 1
    // P3: arr 2, burst 2, prio 2
    // P4: arr 3, burst 1, prio 1
    // Expected: P1(0-5) -> P2(5-8) -> P4(8-9) -> P3(9-11)
    const std::vector<Process> workload = {
        Process("P1", 0, 5, 3),
        Process("P2", 1, 3, 1),
        Process("P3", 2, 2, 2),
        Process("P4", 3, 1, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 4);

    EXPECT_EQ(tl[0].process_id, "P1");
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 5);

    EXPECT_EQ(tl[1].process_id, "P2");
    EXPECT_EQ(tl[1].start_time, 5);
    EXPECT_EQ(tl[1].end_time, 8);

    EXPECT_EQ(tl[2].process_id, "P4");
    EXPECT_EQ(tl[2].start_time, 8);
    EXPECT_EQ(tl[2].end_time, 9);

    EXPECT_EQ(tl[3].process_id, "P3");
    EXPECT_EQ(tl[3].start_time, 9);
    EXPECT_EQ(tl[3].end_time, 11);

    const auto* p1 = res.find_process_state("P1");
    const auto* p2 = res.find_process_state("P2");
    const auto* p3 = res.find_process_state("P3");
    const auto* p4 = res.find_process_state("P4");

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);
    ASSERT_NE(p4, nullptr);

    // P1: ST=0, CT=5, TAT=5, WT=0, RT=0
    EXPECT_EQ(p1->start_time.value(), 0);
    EXPECT_EQ(p1->completion_time, 5);
    EXPECT_EQ(p1->turnaround_time, 5);
    EXPECT_EQ(p1->waiting_time, 0);
    EXPECT_EQ(p1->response_time, 0);

    // P2: ST=5, CT=8, TAT=7, WT=4, RT=4
    EXPECT_EQ(p2->start_time.value(), 5);
    EXPECT_EQ(p2->completion_time, 8);
    EXPECT_EQ(p2->turnaround_time, 7);
    EXPECT_EQ(p2->waiting_time, 4);
    EXPECT_EQ(p2->response_time, 4);

    // P3: ST=9, CT=11, TAT=9, WT=7, RT=7
    EXPECT_EQ(p3->start_time.value(), 9);
    EXPECT_EQ(p3->completion_time, 11);
    EXPECT_EQ(p3->turnaround_time, 9);
    EXPECT_EQ(p3->waiting_time, 7);
    EXPECT_EQ(p3->response_time, 7);

    // P4: ST=8, CT=9, TAT=6, WT=5, RT=5
    EXPECT_EQ(p4->start_time.value(), 8);
    EXPECT_EQ(p4->completion_time, 9);
    EXPECT_EQ(p4->turnaround_time, 6);
    EXPECT_EQ(p4->waiting_time, 5);
    EXPECT_EQ(p4->response_time, 5);

    // Averages:
    // TAT: (5 + 7 + 9 + 6) / 4 = 27 / 4 = 6.75
    // WT:  (0 + 4 + 7 + 5) / 4 = 16 / 4 = 4.0
    // RT:  (0 + 4 + 7 + 5) / 4 = 16 / 4 = 4.0
    EXPECT_NEAR(res.average_turnaround_time(), 6.75, kEpsilon);
    EXPECT_NEAR(res.average_waiting_time(), 4.0, kEpsilon);
    EXPECT_NEAR(res.average_response_time(), 4.0, kEpsilon);
    EXPECT_NEAR(res.cpu_utilization(), 100.0, kEpsilon);

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, ScenarioA_NonPreemptive) {
    // P1: AT=0, BT=3, Prio=2
    // P2: AT=1, BT=2, Prio=1
    // P3: AT=2, BT=4, Prio=1
    // Non-preemptive execution: P1(0-3) -> P2(3-5) -> P3(5-9)
    const std::vector<Process> workload = {
        Process("P1", 0, 3, 2),
        Process("P2", 1, 2, 1),
        Process("P3", 2, 4, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 3);

    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 3]
    EXPECT_EQ(tl[1].process_id, "P2"); // [3, 5]
    EXPECT_EQ(tl[2].process_id, "P3"); // [5, 9]

    const auto* p1 = res.find_process_state("P1");
    const auto* p2 = res.find_process_state("P2");
    const auto* p3 = res.find_process_state("P3");

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);

    EXPECT_EQ(p1->completion_time, 3);
    EXPECT_EQ(p1->turnaround_time, 3);
    EXPECT_EQ(p1->waiting_time, 0);

    EXPECT_EQ(p2->completion_time, 5);
    EXPECT_EQ(p2->turnaround_time, 4);
    EXPECT_EQ(p2->waiting_time, 2);

    EXPECT_EQ(p3->completion_time, 9);
    EXPECT_EQ(p3->turnaround_time, 7);
    EXPECT_EQ(p3->waiting_time, 3);

    EXPECT_NEAR(res.average_turnaround_time(), 14.0 / 3.0, kEpsilon);
    EXPECT_NEAR(res.average_waiting_time(), 5.0 / 3.0, kEpsilon);

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, ScenarioD_NonPreemptive) {
    // P1: AT=4, BT=3, Prio=2
    // P2: AT=12, BT=2, Prio=1
    // Idle(0-4) -> P1(4-7) -> Idle(7-12) -> P2(12-14)
    const std::vector<Process> workload = {
        Process("P1", 4, 3, 2),
        Process("P2", 12, 2, 1)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 4);

    EXPECT_TRUE(tl[0].is_idle());
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 4);

    EXPECT_EQ(tl[1].process_id, "P1");
    EXPECT_EQ(tl[1].start_time, 4);
    EXPECT_EQ(tl[1].end_time, 7);

    EXPECT_TRUE(tl[2].is_idle());
    EXPECT_EQ(tl[2].start_time, 7);
    EXPECT_EQ(tl[2].end_time, 12);

    EXPECT_EQ(tl[3].process_id, "P2");
    EXPECT_EQ(tl[3].start_time, 12);
    EXPECT_EQ(tl[3].end_time, 14);

    EXPECT_EQ(res.total_execution_time(), 14);
    EXPECT_EQ(res.cpu_busy_time(), 5);
    EXPECT_EQ(res.cpu_idle_time(), 9);
    EXPECT_NEAR(res.cpu_utilization(), (5.0 / 14.0) * 100.0, kEpsilon);

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, ScenarioE_NonPreemptive) {
    // P1: AT=0, BT=3, Prio=0
    // P2: AT=0, BT=2, Prio=-5
    // P3: AT=0, BT=1, Prio=3
    // Order: P2 (-5) -> P1 (0) -> P3 (3)
    const std::vector<Process> workload = {
        Process("P1", 0, 3, 0),
        Process("P2", 0, 2, -5),
        Process("P3", 0, 1, 3)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 3);

    EXPECT_EQ(tl[0].process_id, "P2"); // [0, 2]
    EXPECT_EQ(tl[1].process_id, "P1"); // [2, 5]
    EXPECT_EQ(tl[2].process_id, "P3"); // [5, 6]

    verify_timeline_invariants(res, workload);
}

TEST(NonPreemptivePrioritySchedulerTest, StaticRunHelperVectorAndSpan) {
    const std::vector<Process> workload = {
        Process("P1", 0, 2, 1),
        Process("P2", 1, 2, 2)
    };
    const auto outcome_vec = NonPreemptivePriorityScheduler::run(workload);
    const auto outcome_span = NonPreemptivePriorityScheduler::run(std::span<const Process>(workload));

    ASSERT_TRUE(outcome_vec.has_value());
    ASSERT_TRUE(outcome_span.has_value());
    EXPECT_EQ(outcome_vec.value().total_execution_time(), outcome_span.value().total_execution_time());
}

TEST(NonPreemptivePrioritySchedulerTest, FindProcessState) {
    const std::vector<Process> workload = { Process("P1", 0, 3, 1) };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    EXPECT_NE(outcome.value().find_process_state("P1"), nullptr);
    EXPECT_EQ(outcome.value().find_process_state("Unknown"), nullptr);
}

TEST(NonPreemptivePrioritySchedulerTest, AverageMetricsCalculation) {
    const std::vector<Process> workload = {
        Process("P1", 0, 2, 1),
        Process("P2", 0, 4, 2)
    };
    const auto outcome = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    // P1: CT=2, TAT=2, WT=0, RT=0
    // P2: CT=6, TAT=6, WT=2, RT=2
    EXPECT_NEAR(res.average_waiting_time(), 1.0, kEpsilon);
    EXPECT_NEAR(res.average_turnaround_time(), 4.0, kEpsilon);
    EXPECT_NEAR(res.average_response_time(), 1.0, kEpsilon);
}

// ============================================================================
// Preemptive Priority Scheduler Tests (37 tests)
// ============================================================================

TEST(PreemptivePrioritySchedulerTest, SingleProcess) {
    const std::vector<Process> workload = { Process("P1", 0, 5, 2) };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 1);
    EXPECT_EQ(tl[0].process_id, "P1");
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 5);

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, HigherPriorityArrivalCausesPreemption) {
    // P1 starts at 0 with burst 10, prio 2. P2 arrives at 2 with burst 4, prio 1.
    // P2 strictly higher priority (1 < 2) -> preempts P1!
    // P1(0-2) -> P2(2-6) -> P1(6-14)
    const std::vector<Process> workload = {
        Process("P1", 0, 10, 2),
        Process("P2", 2, 4, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 3);

    EXPECT_EQ(tl[0].process_id, "P1");
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 2);

    EXPECT_EQ(tl[1].process_id, "P2");
    EXPECT_EQ(tl[1].start_time, 2);
    EXPECT_EQ(tl[1].end_time, 6);

    EXPECT_EQ(tl[2].process_id, "P1");
    EXPECT_EQ(tl[2].start_time, 6);
    EXPECT_EQ(tl[2].end_time, 14);

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->start_time.value(), 0);
    EXPECT_EQ(p1->completion_time, 14);
    EXPECT_EQ(p1->turnaround_time, 14);
    EXPECT_EQ(p1->waiting_time, 4); // waited during [2, 6]
    EXPECT_EQ(p1->response_time, 0);

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, MultipleSuccessivePreemptions) {
    // P1 starts at 0 (prio 5). P2 arrives at 2 (prio 3) -> preempts P1.
    // P3 arrives at 4 (prio 1) -> preempts P2.
    // P3 finishes, then P2 finishes, then P1 finishes.
    const std::vector<Process> workload = {
        Process("P1", 0, 10, 5),
        Process("P2", 2, 4, 3),
        Process("P3", 4, 2, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 5);

    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 2] (rem 8)
    EXPECT_EQ(tl[1].process_id, "P2"); // [2, 4] (rem 2)
    EXPECT_EQ(tl[2].process_id, "P3"); // [4, 6] (fin)
    EXPECT_EQ(tl[3].process_id, "P2"); // [6, 8] (fin)
    EXPECT_EQ(tl[4].process_id, "P1"); // [8, 16] (fin)

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, EqualPriorityArrivalDoesNotPreempt) {
    // P1 running with prio 2. P2 arrives at 2 with equal prio 2.
    // P1 MUST NOT be preempted. P1 continues to completion.
    const std::vector<Process> workload = {
        Process("P1", 0, 5, 2),
        Process("P2", 2, 3, 2)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 2);

    EXPECT_EQ(tl[0].process_id, "P1");
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 5);

    EXPECT_EQ(tl[1].process_id, "P2");
    EXPECT_EQ(tl[1].start_time, 5);
    EXPECT_EQ(tl[1].end_time, 8);

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, LowerPriorityArrivalDoesNotPreempt) {
    // P1 running with prio 1. P2 arrives at 2 with prio 5.
    // P1 continues to completion.
    const std::vector<Process> workload = {
        Process("P1", 0, 6, 1),
        Process("P2", 2, 2, 5)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 2);

    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 6]
    EXPECT_EQ(tl[1].process_id, "P2"); // [6, 8]

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, PreemptedProcessResumesWithExactRemainingTime) {
    const std::vector<Process> workload = {
        Process("P1", 0, 7, 3),
        Process("P2", 3, 2, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 3);

    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 3] (ran 3, rem 4)
    EXPECT_EQ(tl[1].process_id, "P2"); // [3, 5]
    EXPECT_EQ(tl[2].process_id, "P1"); // [5, 9] (ran remaining 4)

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, MultipleProcessesArrivingSimultaneously) {
    // At t=2, P2(prio 1) and P3(prio 2) arrive simultaneously while P1(prio 3) is running.
    // P2 has highest priority, preempts P1.
    const std::vector<Process> workload = {
        Process("P1", 0, 8, 3),
        Process("P2", 2, 2, 1),
        Process("P3", 2, 3, 2)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 4);

    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 2]
    EXPECT_EQ(tl[1].process_id, "P2"); // [2, 4]
    EXPECT_EQ(tl[2].process_id, "P3"); // [4, 7]
    EXPECT_EQ(tl[3].process_id, "P1"); // [7, 13]

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, ArrivalExactlyAtCompletionTime) {
    // P1 runs [0, 4]. P2 arrives at exactly t=4.
    const std::vector<Process> workload = {
        Process("P1", 0, 4, 2),
        Process("P2", 4, 3, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 2);

    EXPECT_EQ(tl[0].process_id, "P1");
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 4);

    EXPECT_EQ(tl[1].process_id, "P2");
    EXPECT_EQ(tl[1].start_time, 4);
    EXPECT_EQ(tl[1].end_time, 7);

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, ArrivalExactlyAtPreemptionBoundary) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5, 4),
        Process("P2", 2, 2, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& tl = outcome.value().timeline();
    ASSERT_EQ(tl.size(), 3);
    EXPECT_EQ(tl[0].end_time, tl[1].start_time);
    EXPECT_EQ(tl[1].end_time, tl[2].start_time);

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(PreemptivePrioritySchedulerTest, EqualPriorityDispatchTieBreaking) {
    // When dispatching from ready pool, equal priority processes break by arrival time.
    const std::vector<Process> workload = {
        Process("P1", 0, 2, 5),
        Process("P2", 1, 2, 1),
        Process("P3", 2, 2, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    // At t=1, P2(prio 1) preempts P1.
    // [0, 1] P1 (rem 1)
    // [1, 3] P2 (fin). During this, P3 arrives at 2 (prio 1).
    // At t=3, P3(prio 1) vs P1(prio 5). P3 runs [3, 5].
    // Then P1 runs [5, 6].
    ASSERT_EQ(tl.size(), 4);
    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 1]
    EXPECT_EQ(tl[1].process_id, "P2"); // [1, 3]
    EXPECT_EQ(tl[2].process_id, "P3"); // [3, 5]
    EXPECT_EQ(tl[3].process_id, "P1"); // [5, 6]

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, EarlierArrivalWinsEqualPriorityDispatch) {
    const std::vector<Process> workload = {
        Process("P1", 0, 6, 3),
        Process("P2", 1, 2, 1),
        Process("P3", 2, 2, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& tl = outcome.value().timeline();
    // At t=1: P2 preempts P1. P2 runs [1, 3].
    // At t=3: P3 runs [3, 5].
    // At t=5: P1 runs [5, 10].
    ASSERT_EQ(tl.size(), 4);
    EXPECT_EQ(tl[0].process_id, "P1");
    EXPECT_EQ(tl[1].process_id, "P2");
    EXPECT_EQ(tl[2].process_id, "P3");
    EXPECT_EQ(tl[3].process_id, "P1");

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(PreemptivePrioritySchedulerTest, OriginalInputOrderBreaksRemainingTies) {
    // Both arrive at t=1 with equal priority 1. P2 is index 1, P3 is index 2.
    const std::vector<Process> workload = {
        Process("P1", 0, 4, 3),
        Process("P2", 1, 2, 1),
        Process("P3", 1, 2, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& tl = outcome.value().timeline();
    ASSERT_EQ(tl.size(), 4);
    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 1]
    EXPECT_EQ(tl[1].process_id, "P2"); // [1, 3]
    EXPECT_EQ(tl[2].process_id, "P3"); // [3, 5]
    EXPECT_EQ(tl[3].process_id, "P1"); // [5, 8]

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(PreemptivePrioritySchedulerTest, InitialIdleTime) {
    const std::vector<Process> workload = { Process("P1", 3, 4, 1) };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& tl = outcome.value().timeline();
    ASSERT_EQ(tl.size(), 2);
    EXPECT_TRUE(tl[0].is_idle());
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 3);

    EXPECT_EQ(tl[1].process_id, "P1");
    EXPECT_EQ(tl[1].start_time, 3);
    EXPECT_EQ(tl[1].end_time, 7);

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(PreemptivePrioritySchedulerTest, MultipleIdlePeriods) {
    const std::vector<Process> workload = {
        Process("P1", 2, 3, 2),
        Process("P2", 10, 2, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& tl = outcome.value().timeline();
    ASSERT_EQ(tl.size(), 4);
    EXPECT_TRUE(tl[0].is_idle()); // [0, 2]
    EXPECT_EQ(tl[1].process_id, "P1"); // [2, 5]
    EXPECT_TRUE(tl[2].is_idle()); // [5, 10]
    EXPECT_EQ(tl[3].process_id, "P2"); // [10, 12]

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(PreemptivePrioritySchedulerTest, FirstResponseTimeAfterPreemption) {
    // Response time MUST be recorded at initial CPU acquisition, NOT upon resumption
    const std::vector<Process> workload = {
        Process("P1", 0, 6, 2),
        Process("P2", 2, 3, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto* p1 = outcome.value().find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->start_time.value(), 0);
    EXPECT_EQ(p1->response_time, 0); // Started at 0, arrived at 0

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(PreemptivePrioritySchedulerTest, AccumulatedWaitingTime) {
    // P1 runs [0, 2] (rem 6), preempted by P2 [2, 4], preempted by P3 [4, 6].
    // Then P1 resumes [6, 12].
    // P1 burst = 8. CT = 12. TAT = 12. WT = 12 - 8 = 4 (waited during [2, 6]).
    const std::vector<Process> workload = {
        Process("P1", 0, 8, 3),
        Process("P2", 2, 2, 1),
        Process("P3", 4, 2, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto* p1 = outcome.value().find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->waiting_time, 4);

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(PreemptivePrioritySchedulerTest, CompletionMetrics) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4, 2),
        Process("P2", 1, 2, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto* p2 = outcome.value().find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 3);
    EXPECT_EQ(p2->turnaround_time, 2);
    EXPECT_EQ(p2->waiting_time, 0);
    EXPECT_EQ(p2->response_time, 0);

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(PreemptivePrioritySchedulerTest, AverageMetricsCalculation) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4, 2),
        Process("P2", 1, 2, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    // P1: [0, 1] then [3, 6]. CT=6, TAT=6, WT=2, RT=0
    // P2: [1, 3]. CT=3, TAT=2, WT=0, RT=0
    EXPECT_NEAR(res.average_waiting_time(), 1.0, kEpsilon);
    EXPECT_NEAR(res.average_turnaround_time(), 4.0, kEpsilon);
    EXPECT_NEAR(res.average_response_time(), 0.0, kEpsilon);
}

TEST(PreemptivePrioritySchedulerTest, CpuUtilization) {
    const std::vector<Process> workload = {
        Process("P1", 2, 4, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    EXPECT_EQ(res.total_execution_time(), 6);
    EXPECT_EQ(res.cpu_busy_time(), 4);
    EXPECT_EQ(res.cpu_idle_time(), 2);
    EXPECT_NEAR(res.cpu_utilization(), (4.0 / 6.0) * 100.0, kEpsilon);
}

TEST(PreemptivePrioritySchedulerTest, NegativeAndZeroPriorities) {
    // -10 > -1 > 0 > 5
    const std::vector<Process> workload = {
        Process("P1", 0, 6, 5),
        Process("P2", 1, 3, 0),
        Process("P3", 2, 2, -1),
        Process("P4", 3, 1, -10)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& tl = outcome.value().timeline();
    // t=0: P1 starts (prio 5)
    // t=1: P2 arrives (prio 0 < 5) -> preempts P1 [0, 1]
    // t=2: P3 arrives (prio -1 < 0) -> preempts P2 [1, 2]
    // t=3: P4 arrives (prio -10 < -1) -> preempts P3 [2, 3]
    // P4 runs [3, 4] (fin)
    // P3 resumes [4, 5] (fin)
    // P2 resumes [5, 7] (fin)
    // P1 resumes [7, 12] (fin)
    ASSERT_EQ(tl.size(), 7);
    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 1]
    EXPECT_EQ(tl[1].process_id, "P2"); // [1, 2]
    EXPECT_EQ(tl[2].process_id, "P3"); // [2, 3]
    EXPECT_EQ(tl[3].process_id, "P4"); // [3, 4]
    EXPECT_EQ(tl[4].process_id, "P3"); // [4, 5]
    EXPECT_EQ(tl[5].process_id, "P2"); // [5, 7]
    EXPECT_EQ(tl[6].process_id, "P1"); // [7, 12]

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(PreemptivePrioritySchedulerTest, LargePriorityValues) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4, 1000000),
        Process("P2", 1, 2, -1000000)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& tl = outcome.value().timeline();
    ASSERT_EQ(tl.size(), 3);
    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 1]
    EXPECT_EQ(tl[1].process_id, "P2"); // [1, 3]
    EXPECT_EQ(tl[2].process_id, "P1"); // [3, 6]

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(PreemptivePrioritySchedulerTest, LargeArrivalAndBurstValues) {
    constexpr Time large_arr = 2'000'000'000;
    constexpr Time large_burst = 1'000'000'000;
    const std::vector<Process> workload = {
        Process("P1", large_arr, large_burst, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    EXPECT_EQ(res.total_execution_time(), large_arr + large_burst);
    EXPECT_EQ(res.cpu_busy_time(), large_burst);

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, OverflowProtection) {
    const std::vector<Process> workload = {
        Process("P1", std::numeric_limits<Time>::max() - 10, 100, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ArithmeticOverflow);
}

TEST(PreemptivePrioritySchedulerTest, WorkloadImmutability) {
    const std::vector<Process> original = {
        Process("P1", 0, 6, 2),
        Process("P2", 2, 2, 1)
    };
    const std::vector<Process> copy = original;

    const auto outcome = PreemptivePriorityScheduler::run(original);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(original, copy);
}

TEST(PreemptivePrioritySchedulerTest, TimelineIntegrity) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5, 3),
        Process("P2", 1, 3, 1),
        Process("P3", 2, 2, 2)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(PreemptivePrioritySchedulerTest, GanttChartRendering) {
    const std::vector<Process> workload = {
        Process("P1", 0, 4, 2),
        Process("P2", 1, 2, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const std::string chart = GanttChart::render(outcome.value().timeline());
    EXPECT_FALSE(chart.empty());
    EXPECT_NE(chart.find("P1"), std::string::npos);
    EXPECT_NE(chart.find("P2"), std::string::npos);
}

TEST(PreemptivePrioritySchedulerTest, MultiplePreemptionsOfOneProcess) {
    // P1 is preempted by P2, then later preempted by P3
    const std::vector<Process> workload = {
        Process("P1", 0, 10, 5),
        Process("P2", 2, 2, 2),
        Process("P3", 6, 2, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& tl = outcome.value().timeline();
    // [0, 2] P1 (rem 8)
    // [2, 4] P2 (fin)
    // [4, 6] P1 resumes (rem 6)
    // [6, 8] P3 (fin)
    // [8, 14] P1 finishes
    ASSERT_EQ(tl.size(), 5);
    EXPECT_EQ(tl[0].process_id, "P1");
    EXPECT_EQ(tl[1].process_id, "P2");
    EXPECT_EQ(tl[2].process_id, "P1");
    EXPECT_EQ(tl[3].process_id, "P3");
    EXPECT_EQ(tl[4].process_id, "P1");

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(PreemptivePrioritySchedulerTest, NoUnnecessaryPreemption) {
    // Processes arrive with strictly lower priority than running process
    const std::vector<Process> workload = {
        Process("P1", 0, 6, 1),
        Process("P2", 1, 2, 3),
        Process("P3", 2, 2, 4)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& tl = outcome.value().timeline();
    // P1 runs uninterrupted [0, 6]
    ASSERT_EQ(tl.size(), 3);
    EXPECT_EQ(tl[0].process_id, "P1");
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 6);

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(PreemptivePrioritySchedulerTest, CompletionExactlyAtNewArrivalTimestamp) {
    const std::vector<Process> workload = {
        Process("P1", 0, 3, 2),
        Process("P2", 3, 2, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& tl = outcome.value().timeline();
    ASSERT_EQ(tl.size(), 2);
    EXPECT_EQ(tl[0].process_id, "P1");
    EXPECT_EQ(tl[0].end_time, 3);
    EXPECT_EQ(tl[1].process_id, "P2");
    EXPECT_EQ(tl[1].start_time, 3);

    verify_timeline_invariants(outcome.value(), workload);
}

TEST(PreemptivePrioritySchedulerTest, ValidationEmptyWorkload) {
    const std::vector<Process> workload = {};
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
}

TEST(PreemptivePrioritySchedulerTest, MandatoryExample_Section7_4) {
    // From Batch 6 Section 7.4:
    // P1: arr 0, burst 7, prio 3
    // P2: arr 2, burst 4, prio 1
    // P3: arr 4, burst 2, prio 2
    // P4: arr 5, burst 3, prio 1
    // Expected normalized:
    // P1(0-2) -> P2(2-6) -> P4(6-9) -> P3(9-11) -> P1(11-16)
    const std::vector<Process> workload = {
        Process("P1", 0, 7, 3),
        Process("P2", 2, 4, 1),
        Process("P3", 4, 2, 2),
        Process("P4", 5, 3, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 5);

    EXPECT_EQ(tl[0].process_id, "P1");
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 2);

    EXPECT_EQ(tl[1].process_id, "P2");
    EXPECT_EQ(tl[1].start_time, 2);
    EXPECT_EQ(tl[1].end_time, 6);

    EXPECT_EQ(tl[2].process_id, "P4");
    EXPECT_EQ(tl[2].start_time, 6);
    EXPECT_EQ(tl[2].end_time, 9);

    EXPECT_EQ(tl[3].process_id, "P3");
    EXPECT_EQ(tl[3].start_time, 9);
    EXPECT_EQ(tl[3].end_time, 11);

    EXPECT_EQ(tl[4].process_id, "P1");
    EXPECT_EQ(tl[4].start_time, 11);
    EXPECT_EQ(tl[4].end_time, 16);

    const auto* p1 = res.find_process_state("P1");
    const auto* p2 = res.find_process_state("P2");
    const auto* p3 = res.find_process_state("P3");
    const auto* p4 = res.find_process_state("P4");

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);
    ASSERT_NE(p4, nullptr);

    // P1: ST=0, CT=16, TAT=16, WT=9, RT=0
    EXPECT_EQ(p1->start_time.value(), 0);
    EXPECT_EQ(p1->completion_time, 16);
    EXPECT_EQ(p1->turnaround_time, 16);
    EXPECT_EQ(p1->waiting_time, 9);
    EXPECT_EQ(p1->response_time, 0);

    // P2: ST=2, CT=6, TAT=4, WT=0, RT=0
    EXPECT_EQ(p2->start_time.value(), 2);
    EXPECT_EQ(p2->completion_time, 6);
    EXPECT_EQ(p2->turnaround_time, 4);
    EXPECT_EQ(p2->waiting_time, 0);
    EXPECT_EQ(p2->response_time, 0);

    // P3: ST=9, CT=11, TAT=7, WT=5, RT=5
    EXPECT_EQ(p3->start_time.value(), 9);
    EXPECT_EQ(p3->completion_time, 11);
    EXPECT_EQ(p3->turnaround_time, 7);
    EXPECT_EQ(p3->waiting_time, 5);
    EXPECT_EQ(p3->response_time, 5);

    // P4: ST=6, CT=9, TAT=4, WT=1, RT=1
    EXPECT_EQ(p4->start_time.value(), 6);
    EXPECT_EQ(p4->completion_time, 9);
    EXPECT_EQ(p4->turnaround_time, 4);
    EXPECT_EQ(p4->waiting_time, 1);
    EXPECT_EQ(p4->response_time, 1);

    // Averages:
    // TAT: (16 + 4 + 7 + 4) / 4 = 31 / 4 = 7.75
    // WT:  (9 + 0 + 5 + 1) / 4 = 15 / 4 = 3.75
    // RT:  (0 + 0 + 5 + 1) / 4 = 6 / 4 = 1.50
    EXPECT_NEAR(res.average_turnaround_time(), 7.75, kEpsilon);
    EXPECT_NEAR(res.average_waiting_time(), 3.75, kEpsilon);
    EXPECT_NEAR(res.average_response_time(), 1.50, kEpsilon);
    EXPECT_NEAR(res.cpu_utilization(), 100.0, kEpsilon);

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, ScenarioA_Preemptive) {
    // P1: AT=0, BT=3, Prio=2
    // P2: AT=1, BT=2, Prio=1
    // P3: AT=2, BT=4, Prio=1
    // Trace:
    // P1 runs [0, 1] (rem 2)
    // P2 arrives at 1 (prio 1 < 2) -> preempts P1, runs [1, 3] (fin)
    // P3 arrives at 2 (prio 1 == P2 prio 1, no preemption of P2)
    // At t=3: P3(prio 1) vs P1(prio 2) -> P3 runs [3, 7] (fin)
    // P1 resumes [7, 9] (fin)
    const std::vector<Process> workload = {
        Process("P1", 0, 3, 2),
        Process("P2", 1, 2, 1),
        Process("P3", 2, 4, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 4);

    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 1]
    EXPECT_EQ(tl[1].process_id, "P2"); // [1, 3]
    EXPECT_EQ(tl[2].process_id, "P3"); // [3, 7]
    EXPECT_EQ(tl[3].process_id, "P1"); // [7, 9]

    const auto* p1 = res.find_process_state("P1");
    const auto* p2 = res.find_process_state("P2");
    const auto* p3 = res.find_process_state("P3");

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);

    EXPECT_EQ(p1->start_time.value(), 0);
    EXPECT_EQ(p1->completion_time, 9);
    EXPECT_EQ(p1->turnaround_time, 9);
    EXPECT_EQ(p1->waiting_time, 6);
    EXPECT_EQ(p1->response_time, 0);

    EXPECT_EQ(p2->start_time.value(), 1);
    EXPECT_EQ(p2->completion_time, 3);
    EXPECT_EQ(p2->turnaround_time, 2);
    EXPECT_EQ(p2->waiting_time, 0);
    EXPECT_EQ(p2->response_time, 0);

    EXPECT_EQ(p3->start_time.value(), 3);
    EXPECT_EQ(p3->completion_time, 7);
    EXPECT_EQ(p3->turnaround_time, 5);
    EXPECT_EQ(p3->waiting_time, 1);
    EXPECT_EQ(p3->response_time, 1);

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, ScenarioB_Preemptive) {
    // P1: AT=0, BT=10, Prio=5
    // P2: AT=2, BT=5, Prio=3
    // P3: AT=4, BT=3, Prio=1
    // P4: AT=6, BT=2, Prio=2
    // Trace:
    // P1(0-2) -> P2(2-4) -> P3(4-7) -> P4(7-9) -> P2(9-12) -> P1(12-20)
    const std::vector<Process> workload = {
        Process("P1", 0, 10, 5),
        Process("P2", 2, 5, 3),
        Process("P3", 4, 3, 1),
        Process("P4", 6, 2, 2)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 6);

    EXPECT_EQ(tl[0].process_id, "P1"); // [0, 2]
    EXPECT_EQ(tl[1].process_id, "P2"); // [2, 4]
    EXPECT_EQ(tl[2].process_id, "P3"); // [4, 7]
    EXPECT_EQ(tl[3].process_id, "P4"); // [7, 9]
    EXPECT_EQ(tl[4].process_id, "P2"); // [9, 12]
    EXPECT_EQ(tl[5].process_id, "P1"); // [12, 20]

    const auto* p1 = res.find_process_state("P1");
    const auto* p2 = res.find_process_state("P2");
    const auto* p3 = res.find_process_state("P3");
    const auto* p4 = res.find_process_state("P4");

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);
    ASSERT_NE(p4, nullptr);

    EXPECT_EQ(p1->completion_time, 20);
    EXPECT_EQ(p1->turnaround_time, 20);
    EXPECT_EQ(p1->waiting_time, 10);
    EXPECT_EQ(p1->response_time, 0);

    EXPECT_EQ(p2->completion_time, 12);
    EXPECT_EQ(p2->turnaround_time, 10);
    EXPECT_EQ(p2->waiting_time, 5);
    EXPECT_EQ(p2->response_time, 0);

    EXPECT_EQ(p3->completion_time, 7);
    EXPECT_EQ(p3->turnaround_time, 3);
    EXPECT_EQ(p3->waiting_time, 0);
    EXPECT_EQ(p3->response_time, 0);

    EXPECT_EQ(p4->completion_time, 9);
    EXPECT_EQ(p4->turnaround_time, 3);
    EXPECT_EQ(p4->waiting_time, 1);
    EXPECT_EQ(p4->response_time, 1);

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, ScenarioC_Preemptive) {
    // P1: AT=0, BT=6, Prio=2
    // P2: AT=2, BT=2, Prio=2
    // P3: AT=3, BT=1, Prio=1
    // Trace:
    // P1 runs [0, 2]. At t=2, P2 arrives with equal prio 2 -> no preemption!
    // P1 runs [2, 3]. At t=3, P3 arrives with prio 1 < 2 -> preempts P1!
    // P1 segment [0, 3] emitted.
    // P3 runs [3, 4] (fin).
    // At t=4, ready queue has P1(prio 2, arr 0) and P2(prio 2, arr 2).
    // Equal prio -> earlier arrival wins: P1 runs [4, 7] (fin).
    // P2 runs [7, 9] (fin).
    const std::vector<Process> workload = {
        Process("P1", 0, 6, 2),
        Process("P2", 2, 2, 2),
        Process("P3", 3, 1, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 4);

    EXPECT_EQ(tl[0].process_id, "P1");
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 3);

    EXPECT_EQ(tl[1].process_id, "P3");
    EXPECT_EQ(tl[1].start_time, 3);
    EXPECT_EQ(tl[1].end_time, 4);

    EXPECT_EQ(tl[2].process_id, "P1");
    EXPECT_EQ(tl[2].start_time, 4);
    EXPECT_EQ(tl[2].end_time, 7);

    EXPECT_EQ(tl[3].process_id, "P2");
    EXPECT_EQ(tl[3].start_time, 7);
    EXPECT_EQ(tl[3].end_time, 9);

    const auto* p1 = res.find_process_state("P1");
    const auto* p2 = res.find_process_state("P2");
    const auto* p3 = res.find_process_state("P3");

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);

    EXPECT_EQ(p1->completion_time, 7);
    EXPECT_EQ(p1->turnaround_time, 7);
    EXPECT_EQ(p1->waiting_time, 1);
    EXPECT_EQ(p1->response_time, 0);

    EXPECT_EQ(p2->completion_time, 9);
    EXPECT_EQ(p2->turnaround_time, 7);
    EXPECT_EQ(p2->waiting_time, 5);
    EXPECT_EQ(p2->response_time, 5);

    EXPECT_EQ(p3->completion_time, 4);
    EXPECT_EQ(p3->turnaround_time, 1);
    EXPECT_EQ(p3->waiting_time, 0);
    EXPECT_EQ(p3->response_time, 0);

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, ScenarioD_Preemptive) {
    // P1: AT=4, BT=3, Prio=2
    // P2: AT=12, BT=2, Prio=1
    // Idle(0-4) -> P1(4-7) -> Idle(7-12) -> P2(12-14)
    const std::vector<Process> workload = {
        Process("P1", 4, 3, 2),
        Process("P2", 12, 2, 1)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 4);

    EXPECT_TRUE(tl[0].is_idle());
    EXPECT_EQ(tl[0].start_time, 0);
    EXPECT_EQ(tl[0].end_time, 4);

    EXPECT_EQ(tl[1].process_id, "P1");
    EXPECT_EQ(tl[1].start_time, 4);
    EXPECT_EQ(tl[1].end_time, 7);

    EXPECT_TRUE(tl[2].is_idle());
    EXPECT_EQ(tl[2].start_time, 7);
    EXPECT_EQ(tl[2].end_time, 12);

    EXPECT_EQ(tl[3].process_id, "P2");
    EXPECT_EQ(tl[3].start_time, 12);
    EXPECT_EQ(tl[3].end_time, 14);

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, ScenarioE_Preemptive) {
    // P1: AT=0, BT=3, Prio=0
    // P2: AT=0, BT=2, Prio=-5
    // P3: AT=0, BT=1, Prio=3
    // Order: P2 (-5) -> P1 (0) -> P3 (3)
    const std::vector<Process> workload = {
        Process("P1", 0, 3, 0),
        Process("P2", 0, 2, -5),
        Process("P3", 0, 1, 3)
    };
    const auto outcome = PreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());

    const auto& res = outcome.value();
    const auto& tl = res.timeline();
    ASSERT_EQ(tl.size(), 3);

    EXPECT_EQ(tl[0].process_id, "P2"); // [0, 2]
    EXPECT_EQ(tl[1].process_id, "P1"); // [2, 5]
    EXPECT_EQ(tl[2].process_id, "P3"); // [5, 6]

    verify_timeline_invariants(res, workload);
}

TEST(PreemptivePrioritySchedulerTest, StaticRunHelperVectorAndSpan) {
    const std::vector<Process> workload = {
        Process("P1", 0, 3, 2),
        Process("P2", 1, 2, 1)
    };
    const auto outcome_vec = PreemptivePriorityScheduler::run(workload);
    const auto outcome_span = PreemptivePriorityScheduler::run(std::span<const Process>(workload));

    ASSERT_TRUE(outcome_vec.has_value());
    ASSERT_TRUE(outcome_span.has_value());
    EXPECT_EQ(outcome_vec.value().total_execution_time(), outcome_span.value().total_execution_time());
}

} // namespace
} // namespace cpu_scheduler
