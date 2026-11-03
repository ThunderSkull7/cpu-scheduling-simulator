#include "cpu_scheduler/fcfs_scheduler.hpp"
#include "cpu_scheduler/mlfq_scheduler.hpp"
#include "cpu_scheduler/mlq_scheduler.hpp"
#include "cpu_scheduler/priority_scheduler.hpp"
#include "cpu_scheduler/rr_scheduler.hpp"
#include "cpu_scheduler/scheduling_engine.hpp"
#include "cpu_scheduler/sjf_scheduler.hpp"
#include "cpu_scheduler/srtf_scheduler.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <numeric>
#include <string>
#include <vector>

namespace cpu_scheduler {
namespace {

/**
 * @brief Universal scheduling invariant verification helper.
 *
 * Verifies the fundamental mathematical and physical invariants that must hold
 * for any valid, completed CPU schedule regardless of the algorithm used.
 */
void verify_scheduling_invariants(
    const std::vector<Process>& workload,
    const SchedulingResult& result) {

    // 1. Process count and ID completeness
    ASSERT_EQ(result.processes().size(), workload.size());
    std::map<std::string, const Process*> original_map;
    for (const auto& proc : workload) {
        original_map[proc.id] = &proc;
    }

    std::map<std::string, Time> total_executed_duration;
    std::map<std::string, Time> first_start_time;
    std::map<std::string, Time> last_end_time;

    Time total_busy_from_segments = 0;
    Time total_idle_from_segments = 0;

    // 2. Timeline segment continuity, ordering, and non-overlapping
    Time current_timeline_cursor = 0;
    if (!result.timeline().empty()) {
        current_timeline_cursor = result.timeline().front().start_time;
        // First segment must start at or after time 0
        EXPECT_GE(current_timeline_cursor, 0);
    }

    for (std::size_t i = 0; i < result.timeline().size(); ++i) {
        const auto& seg = result.timeline()[i];

        // Start time strictly precedes end time (positive duration)
        EXPECT_LT(seg.start_time, seg.end_time);
        Time duration = seg.duration();
        EXPECT_GT(duration, 0);

        // Segments must be chronological and strictly contiguous
        EXPECT_EQ(seg.start_time, current_timeline_cursor);
        current_timeline_cursor = seg.end_time;

        if (seg.is_idle()) {
            total_idle_from_segments += duration;
            EXPECT_FALSE(seg.process_id.has_value());
        } else {
            ASSERT_TRUE(seg.process_id.has_value());
            const std::string& pid = *seg.process_id;
            ASSERT_TRUE(original_map.contains(pid));

            total_busy_from_segments += duration;
            total_executed_duration[pid] += duration;

            if (!first_start_time.contains(pid)) {
                first_start_time[pid] = seg.start_time;
            }
            last_end_time[pid] = seg.end_time;

            // Process must never execute before its arrival time
            EXPECT_GE(seg.start_time, original_map[pid]->arrival_time);
        }
    }

    // 3. Per-process metric invariants
    Time total_workload_burst = 0;
    for (const auto& proc_state : result.processes()) {
        ASSERT_TRUE(original_map.contains(proc_state.process_id));
        const auto* orig = original_map[proc_state.process_id];

        total_workload_burst += orig->burst_time;

        // Arrival and burst preserved
        EXPECT_EQ(proc_state.arrival_time, orig->arrival_time);
        EXPECT_EQ(proc_state.burst_time, orig->burst_time);

        // Turnaround = Completion - Arrival
        EXPECT_EQ(proc_state.turnaround_time, proc_state.completion_time - proc_state.arrival_time);

        // Waiting = Turnaround - Burst
        EXPECT_EQ(proc_state.waiting_time, proc_state.turnaround_time - proc_state.burst_time);
        EXPECT_GE(proc_state.waiting_time, 0);

        // Response = First Execution Start - Arrival
        EXPECT_GE(proc_state.response_time, 0);
        EXPECT_LE(proc_state.response_time, proc_state.waiting_time);

        // Completion time must match the end of its final execution segment
        if (last_end_time.contains(proc_state.process_id)) {
            EXPECT_EQ(proc_state.completion_time, last_end_time[proc_state.process_id]);
        }
        if (first_start_time.contains(proc_state.process_id)) {
            EXPECT_EQ(proc_state.response_time, first_start_time[proc_state.process_id] - proc_state.arrival_time);
        }

        // Total execution duration must equal required burst time
        EXPECT_EQ(total_executed_duration[proc_state.process_id], orig->burst_time);
    }

    // 4. Aggregate metrics consistency
    EXPECT_EQ(total_busy_from_segments, total_workload_burst);
    EXPECT_EQ(result.metrics().cpu_busy_time, total_busy_from_segments);
    EXPECT_EQ(result.metrics().cpu_idle_time, total_idle_from_segments);

    Time total_time = result.metrics().total_execution_time;
    EXPECT_EQ(total_time, total_busy_from_segments + total_idle_from_segments);

    if (total_time > 0) {
        double expected_util = (static_cast<double>(total_busy_from_segments) / static_cast<double>(total_time)) * 100.0;
        EXPECT_NEAR(result.metrics().cpu_utilization, expected_util, 0.01);
    }
}

// ============================================================================
// 1. Cross-Algorithm Consistency on Common Workloads
// ============================================================================

TEST(SchedulingIntegrationTest, StandardWorkloadAcrossAllEightAlgorithms) {
    // Workload with mixed arrival times, bursts, and priorities
    const std::vector<Process> workload = {
        Process("P1", 0, 6, 2),
        Process("P2", 2, 4, 1),
        Process("P3", 4, 2, 3),
        Process("P4", 5, 5, 1)
    };

    // 1. FCFS
    {
        auto res = FcfsScheduler::run(workload);
        ASSERT_TRUE(res.has_value());
        verify_scheduling_invariants(workload, res.value());
    }

    // 2. SJF (Non-preemptive)
    {
        auto res = SjfScheduler::run(workload);
        ASSERT_TRUE(res.has_value());
        verify_scheduling_invariants(workload, res.value());
    }

    // 3. SRTF (Preemptive)
    {
        auto res = SrtfScheduler::run(workload);
        ASSERT_TRUE(res.has_value());
        verify_scheduling_invariants(workload, res.value());
    }

    // 4. Round Robin (q=2)
    {
        auto res = RrScheduler::run(workload, 2);
        ASSERT_TRUE(res.has_value());
        verify_scheduling_invariants(workload, res.value());
    }

    // 5. Non-preemptive Priority
    {
        auto res = NonPreemptivePriorityScheduler::run(workload);
        ASSERT_TRUE(res.has_value());
        verify_scheduling_invariants(workload, res.value());
    }

    // 6. Preemptive Priority
    {
        auto res = PreemptivePriorityScheduler::run(workload);
        ASSERT_TRUE(res.has_value());
        verify_scheduling_invariants(workload, res.value());
    }

    // 7. MLQ (2 queues: Q0 Priority 0 RR, Q1 Priority 1 FCFS)
    {
        MlqWorkload mlq;
        mlq.set_inter_queue_policy(InterQueuePolicy::FixedPriorityPreemptive)
           .add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::RoundRobin, 2))
           .add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs))
           .add_process(workload[0], "Q0")
           .add_process(workload[1], "Q0")
           .add_process(workload[2], "Q1")
           .add_process(workload[3], "Q1");

        auto res = MlqScheduler::run(mlq);
        ASSERT_TRUE(res.has_value());
        verify_scheduling_invariants(workload, res.value());
    }

    // 8. MLFQ (3 levels: {2, 4, 8}, boost interval 15)
    {
        MlfqConfig cfg({2, 4, 8}, 15);
        auto res = MlfqScheduler::run(workload, cfg);
        ASSERT_TRUE(res.has_value());
        verify_scheduling_invariants(workload, res.value());
    }
}

// ============================================================================
// 2. Edge Case: Single Process Workload Invariants
// ============================================================================

TEST(SchedulingIntegrationTest, SingleProcessWorkloadInvariants) {
    const std::vector<Process> workload = { Process("Solo", 3, 5, 1) };

    // FCFS
    auto res_fcfs = FcfsScheduler::run(workload);
    ASSERT_TRUE(res_fcfs.has_value());
    verify_scheduling_invariants(workload, res_fcfs.value());
    EXPECT_EQ(res_fcfs.value().processes()[0].waiting_time, 0);
    EXPECT_EQ(res_fcfs.value().processes()[0].response_time, 0);
    EXPECT_EQ(res_fcfs.value().processes()[0].completion_time, 8);
    EXPECT_EQ(res_fcfs.value().metrics().cpu_idle_time, 3); // Idle [0-3]

    // SRTF
    auto res_srtf = SrtfScheduler::run(workload);
    ASSERT_TRUE(res_srtf.has_value());
    verify_scheduling_invariants(workload, res_srtf.value());

    // Round Robin (q=2: should slice into [3-5], [5-7], [7-8])
    auto res_rr = RrScheduler::run(workload, 2);
    ASSERT_TRUE(res_rr.has_value());
    verify_scheduling_invariants(workload, res_rr.value());
    EXPECT_EQ(res_rr.value().processes()[0].completion_time, 8);
}

// ============================================================================
// 3. Edge Case: Simultaneous Arrival at Time Zero
// ============================================================================

TEST(SchedulingIntegrationTest, SimultaneousArrivalAtTimeZero) {
    const std::vector<Process> workload = {
        Process("P1", 0, 5, 3),
        Process("P2", 0, 2, 1),
        Process("P3", 0, 8, 2),
        Process("P4", 0, 1, 4)
    };

    // SJF must pick shortest first: P4(1), P2(2), P1(5), P3(8)
    auto res_sjf = SjfScheduler::run(workload);
    ASSERT_TRUE(res_sjf.has_value());
    verify_scheduling_invariants(workload, res_sjf.value());
    EXPECT_EQ(res_sjf.value().processes()[3].completion_time, 1);  // P4 finishes at 1
    EXPECT_EQ(res_sjf.value().processes()[1].completion_time, 3);  // P2 finishes at 3
    EXPECT_EQ(res_sjf.value().processes()[0].completion_time, 8);  // P1 finishes at 8
    EXPECT_EQ(res_sjf.value().processes()[2].completion_time, 16); // P3 finishes at 16

    // Priority non-preemptive must pick lowest numerical priority first: P2(1), P3(2), P1(3), P4(4)
    auto res_prio = NonPreemptivePriorityScheduler::run(workload);
    ASSERT_TRUE(res_prio.has_value());
    verify_scheduling_invariants(workload, res_prio.value());
    EXPECT_EQ(res_prio.value().processes()[1].completion_time, 2);  // P2 finishes at 2
    EXPECT_EQ(res_prio.value().processes()[2].completion_time, 10); // P3 finishes at 10
    EXPECT_EQ(res_prio.value().processes()[0].completion_time, 15); // P1 finishes at 15
    EXPECT_EQ(res_prio.value().processes()[3].completion_time, 16); // P4 finishes at 16
}

// ============================================================================
// 4. Edge Case: Staggered Arrivals with Intervening Idle Gaps
// ============================================================================

TEST(SchedulingIntegrationTest, StaggeredArrivalsWithMultipleIdlePeriods) {
    const std::vector<Process> workload = {
        Process("A", 2, 3),   // Exec [2-5], Idle [0-2]
        Process("B", 10, 2),  // Exec [10-12], Idle [5-10]
        Process("C", 20, 4)   // Exec [20-24], Idle [12-20]
    };

    auto res = FcfsScheduler::run(workload);
    ASSERT_TRUE(res.has_value());
    verify_scheduling_invariants(workload, res.value());

    // Total idle time must be 2 + 5 + 8 = 15
    EXPECT_EQ(res.value().metrics().cpu_idle_time, 15);
    EXPECT_EQ(res.value().metrics().cpu_busy_time, 9);
    EXPECT_EQ(res.value().metrics().total_execution_time, 24);
}

// ============================================================================
// 5. Heavy Preemption Cascading Scenario (SRTF & Preemptive Priority)
// ============================================================================

TEST(SchedulingIntegrationTest, HeavyPreemptionCascadingScenario) {
    // P_Long arrives at 0 with burst 20.
    // Short jobs arrive sequentially at 2, 4, 6, 8 each with burst 1.
    const std::vector<Process> workload = {
        Process("P_Long", 0, 20),
        Process("S1", 2, 1),
        Process("S2", 4, 1),
        Process("S3", 6, 1),
        Process("S4", 8, 1)
    };

    auto res = SrtfScheduler::run(workload);
    ASSERT_TRUE(res.has_value());
    verify_scheduling_invariants(workload, res.value());

    // S1, S2, S3, S4 must preempt P_Long and complete immediately
    EXPECT_EQ(res.value().processes()[1].completion_time, 3);
    EXPECT_EQ(res.value().processes()[2].completion_time, 5);
    EXPECT_EQ(res.value().processes()[3].completion_time, 7);
    EXPECT_EQ(res.value().processes()[4].completion_time, 9);
    // P_Long completes at 20 + 4 = 24
    EXPECT_EQ(res.value().processes()[0].completion_time, 24);
}

// ============================================================================
// 6. MLFQ Dynamic Demotion and Starvation Boost Invariants
// ============================================================================

TEST(SchedulingIntegrationTest, MlfqDemotionAndStarvationBoostInvariants) {
    // A long process arrives at 0. A batch of short processes arrives later.
    // Boost restores all processes to top level at interval.
    const std::vector<Process> workload = {
        Process("P_Batch", 0, 30),
        Process("P_Interactive1", 10, 2),
        Process("P_Interactive2", 15, 2)
    };

    MlfqConfig cfg({2, 4, 8}, 12); // Boost every 12 time units
    auto res = MlfqScheduler::run(workload, cfg);
    ASSERT_TRUE(res.has_value());
    verify_scheduling_invariants(workload, res.value());

    // All processes successfully terminate and aggregate metrics match
    EXPECT_EQ(res.value().metrics().cpu_busy_time, 34);
    EXPECT_EQ(res.value().metrics().cpu_idle_time, 0);
}

// ============================================================================
// 7. Large Workload Stress (100 Processes Maximum Boundary)
// ============================================================================

TEST(SchedulingIntegrationTest, MaxAllowedWorkload100ProcessesExecution) {
    std::vector<Process> workload;
    workload.reserve(100);
    for (int i = 1; i <= 100; ++i) {
        workload.emplace_back("P" + std::to_string(i), (i - 1) % 10, (i % 5) + 1, (i % 3));
    }

    // Execute via Unified Scheduling Engine
    auto req = SchedulingRequest::round_robin(workload, 2);
    auto outcome = SchedulingEngine::execute(req);

    ASSERT_TRUE(outcome.has_value());
    verify_scheduling_invariants(workload, outcome.value());
    EXPECT_EQ(outcome.value().processes().size(), 100u);
}

// ============================================================================
// 8. Deterministic Reproducibility Invariant
// ============================================================================

TEST(SchedulingIntegrationTest, DeterministicReproducibilityAcrossRepeatedRuns) {
    const std::vector<Process> workload = {
        Process("P1", 0, 7, 2),
        Process("P2", 1, 4, 1),
        Process("P3", 3, 2, 3)
    };

    auto req = SchedulingRequest::preemptive_priority(workload);

    auto outcome1 = SchedulingEngine::execute(req);
    auto outcome2 = SchedulingEngine::execute(req);

    ASSERT_TRUE(outcome1.has_value());
    ASSERT_TRUE(outcome2.has_value());

    EXPECT_EQ(outcome1.value().processes(), outcome2.value().processes());
    EXPECT_EQ(outcome1.value().metrics(), outcome2.value().metrics());
    EXPECT_EQ(outcome1.value().timeline(), outcome2.value().timeline());
}

} // namespace
} // namespace cpu_scheduler
