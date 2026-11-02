#include <gtest/gtest.h>

#include "cpu_scheduler/mlq_scheduler.hpp"
#include "cpu_scheduler/process.hpp"
#include "cpu_scheduler/process_state.hpp"
#include "cpu_scheduler/execution_segment.hpp"
#include "cpu_scheduler/process_validator.hpp"

#include <string>
#include <vector>

using namespace cpu_scheduler;

// ============================================================================
// Group 1: Configuration and Validation Tests (18 tests)
// ============================================================================

TEST(MlqValidationTest, EmptyWorkloadProcessesReturnsError) {
    MlqWorkload workload;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    // No processes added

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlqValidationTest, EmptyQueuesReturnsError) {
    MlqWorkload workload;
    workload.add_process(Process("P1", 0, 5), "Q0");
    // No queues added

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlqValidationTest, DuplicateQueueIdReturnsError) {
    MlqWorkload workload;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q0", 1, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 5), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlqValidationTest, DuplicateQueueIdCaseInsensitiveReturnsError) {
    MlqWorkload workload;
    workload.add_queue(MlqQueueConfig("QueueA", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("queuea", 1, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 5), "QueueA");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlqValidationTest, BlankQueueIdReturnsError) {
    MlqWorkload workload;
    workload.add_queue(MlqQueueConfig("   ", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 5), "   ");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlqValidationTest, MissingQueueAssignmentReturnsError) {
    MlqWorkload workload;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 5), "Q_NonExistent");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlqValidationTest, EmptyProcessQueueBindingReturnsError) {
    MlqWorkload workload;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 5), "  ");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlqValidationTest, ZeroInternalQuantumForRoundRobinReturnsError) {
    MlqWorkload workload;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::RoundRobin, 0));
    workload.add_process(Process("P1", 0, 5), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlqValidationTest, NegativeInternalQuantumForRoundRobinReturnsError) {
    MlqWorkload workload;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::RoundRobin, -5));
    workload.add_process(Process("P1", 0, 5), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlqValidationTest, ZeroInterQueueQuantumForRoundRobinPolicyReturnsError) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 0;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 5), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlqValidationTest, NegativePerQueueInterQueueQuantumReturnsError) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 2;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs, 1, -3));
    workload.add_process(Process("P1", 0, 5), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlqValidationTest, DuplicateProcessIdInWorkloadReturnsError) {
    MlqWorkload workload;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 5), "Q0");
    workload.add_process(Process("p1", 2, 3), "Q0"); // Duplicate P1 case-insensitive

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlqValidationTest, NegativeArrivalTimeReturnsError) {
    MlqWorkload workload;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", -2, 5), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlqValidationTest, ZeroBurstTimeReturnsError) {
    MlqWorkload workload;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 0), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_FALSE(outcome.error().message.empty());
}

TEST(MlqValidationTest, EqualQueuePrioritiesIsValid) {
    MlqWorkload workload;
    workload.add_queue(MlqQueueConfig("Q0", 1, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 5), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    EXPECT_TRUE(outcome.has_value());
}

TEST(MlqValidationTest, NegativeQueuePriorityIsValid) {
    MlqWorkload workload;
    workload.add_queue(MlqQueueConfig("Q0", -5, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 5), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    EXPECT_TRUE(outcome.has_value());
}

TEST(MlqValidationTest, CaseInsensitiveQueueBindingMatches) {
    MlqWorkload workload;
    workload.add_queue(MlqQueueConfig("HighPriority", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 4), "highpriority");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto* p1 = outcome.value().find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->queue_id, "HighPriority");
}

TEST(MlqValidationTest, WorkloadFluentBuilderChaining) {
    MlqWorkload workload;
    workload.set_inter_queue_policy(InterQueuePolicy::FixedPriorityPreemptive)
            .set_default_inter_queue_quantum(4)
            .add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs))
            .add_process(Process("P1", 0, 3), "Q0");

    EXPECT_EQ(workload.inter_queue_policy, InterQueuePolicy::FixedPriorityPreemptive);
    EXPECT_EQ(workload.default_inter_queue_quantum, 4);
    EXPECT_EQ(workload.queues.size(), 1);
    EXPECT_EQ(workload.processes.size(), 1);

    const auto outcome = MlqScheduler::run(workload);
    EXPECT_TRUE(outcome.has_value());
}

// ============================================================================
// Group 2: Fixed-Priority Inter-Queue Policy Tests (22 tests)
// ============================================================================

// Scenario A: Fixed-priority inter-queue preemption
// Q0: pri 0, FCFS; Q1: pri 1, FCFS
// P1: AT=0, BT=8, Q1; P2: AT=2, BT=3, Q0
// Expected: P1(0-2) -> P2(2-5) -> P1(5-11)
TEST(MlqFixedPriorityTest, ScenarioA_PreemptionOfLowerPriorityQueue) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 8), "Q1");
    workload.add_process(Process("P2", 2, 3), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 5));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 5, 11));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 11);
    EXPECT_EQ(p1->turnaround_time, 11);
    EXPECT_EQ(p1->waiting_time, 3);
    EXPECT_EQ(p1->response_time, 0);
    EXPECT_EQ(p1->queue_id, "Q1");

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 5);
    EXPECT_EQ(p2->turnaround_time, 3);
    EXPECT_EQ(p2->waiting_time, 0);
    EXPECT_EQ(p2->response_time, 0);
    EXPECT_EQ(p2->queue_id, "Q0");
}

// Scenario B: Fixed-priority queue remains active
// Q0: pri 0, FCFS; Q1: pri 1, FCFS
// P1: AT=0, BT=5, Q0; P2: AT=1, BT=3, Q1; P3: AT=2, BT=2, Q1
// Expected: P1(0-5) -> P2(5-8) -> P3(8-10)
TEST(MlqFixedPriorityTest, ScenarioB_HigherPriorityQueueRemainsActive) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 5), "Q0");
    workload.add_process(Process("P2", 1, 3), "Q1");
    workload.add_process(Process("P3", 2, 2), "Q1");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 5, 8));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 8, 10));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 5);
    EXPECT_EQ(p1->turnaround_time, 5);
    EXPECT_EQ(p1->waiting_time, 0);
    EXPECT_EQ(p1->response_time, 0);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 8);
    EXPECT_EQ(p2->turnaround_time, 7);
    EXPECT_EQ(p2->waiting_time, 4);
    EXPECT_EQ(p2->response_time, 4);

    const auto* p3 = res.find_process_state("P3");
    ASSERT_NE(p3, nullptr);
    EXPECT_EQ(p3->completion_time, 10);
    EXPECT_EQ(p3->turnaround_time, 8);
    EXPECT_EQ(p3->waiting_time, 6);
    EXPECT_EQ(p3->response_time, 6);
}

// Scenario F: Simultaneous queue activation under fixed priority
// Q0: pri 0, FCFS; Q1: pri 1, FCFS
// P1: AT=0, BT=3, Q1; P2: AT=0, BT=2, Q0
// Expected: P2(0-2) -> P1(2-5)
TEST(MlqFixedPriorityTest, ScenarioF_SimultaneousQueueActivationFixedPriority) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 3), "Q1");
    workload.add_process(Process("P2", 0, 2), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P2", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 5));
}

// Scenario G: Lower-priority queue resumption after multiple interruptions
// Q0: pri 0, FCFS; Q1: pri 1, FCFS
// P1: AT=0, BT=10, Q1
// P2: AT=2, BT=2, Q0
// P3: AT=6, BT=2, Q0
// Expected: P1(0-2) -> P2(2-4) -> P1(4-6) -> P3(6-8) -> P1(8-14)
TEST(MlqFixedPriorityTest, ScenarioG_LowerPriorityQueueResumptionMultiplePreemptions) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 10), "Q1");
    workload.add_process(Process("P2", 2, 2), "Q0");
    workload.add_process(Process("P3", 6, 2), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 5);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P3", 6, 8));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 8, 14));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 14);
    EXPECT_EQ(p1->turnaround_time, 14);
    EXPECT_EQ(p1->waiting_time, 4);
    EXPECT_EQ(p1->response_time, 0);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 4);
    EXPECT_EQ(p2->turnaround_time, 2);
    EXPECT_EQ(p2->waiting_time, 0);
    EXPECT_EQ(p2->response_time, 0);

    const auto* p3 = res.find_process_state("P3");
    ASSERT_NE(p3, nullptr);
    EXPECT_EQ(p3->completion_time, 8);
    EXPECT_EQ(p3->turnaround_time, 2);
    EXPECT_EQ(p3->waiting_time, 0);
    EXPECT_EQ(p3->response_time, 0);
}

// Scenario H: Equal queue priority deterministic tie-breaking
// Q0: pri 1, FCFS; Q1: pri 1, FCFS
// P1: AT=0, BT=3, Q0; P2: AT=0, BT=3, Q1
// Tie-breaking: Q0 was configured first, so Q0 runs P1 completely first
TEST(MlqFixedPriorityTest, ScenarioH_EqualQueuePriorityDeterministicOrder) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 1, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 3), "Q0");
    workload.add_process(Process("P2", 0, 3), "Q1");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 3, 6));
}

TEST(MlqFixedPriorityTest, ThreeQueuesCascadingPreemption) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("High", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Mid", 5, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Low", 10, QueueAlgorithm::Fcfs));

    workload.add_process(Process("PLow", 0, 10), "Low");
    workload.add_process(Process("PMid", 2, 4), "Mid");
    workload.add_process(Process("PHigh", 3, 2), "High");

    // PLow runs 0..2 (rem=8)
    // PMid arrives at 2, runs 2..3 (rem=3)
    // PHigh arrives at 3, runs 3..5 (finishes)
    // PMid resumes 5..8 (finishes)
    // PLow resumes 8..16 (finishes)
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 5);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("PLow", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("PMid", 2, 3));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("PHigh", 3, 5));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("PMid", 5, 8));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("PLow", 8, 16));
}

TEST(MlqFixedPriorityTest, HighPriorityArrivesAtExactCompletionBoundary) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P1", 0, 4), "Q1");
    workload.add_process(Process("P2", 4, 3), "Q0"); // arrives exactly when P1 finishes

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 4));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 4, 7));
}

TEST(MlqFixedPriorityTest, NegativeQueuePriorityHigherThanZero) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q_Zero", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q_Neg", -1, QueueAlgorithm::Fcfs)); // Higher priority!

    workload.add_process(Process("P0", 0, 5), "Q_Zero");
    workload.add_process(Process("PNeg", 2, 2), "Q_Neg");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P0", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("PNeg", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P0", 4, 7));
}

TEST(MlqFixedPriorityTest, IdleIntervalBeforeFirstArrival) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 4, 3), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::idle(0, 4));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 4, 7));
    EXPECT_EQ(res.cpu_idle_time(), 4);
    EXPECT_EQ(res.cpu_busy_time(), 3);
}

TEST(MlqFixedPriorityTest, IdleIntervalBetweenQueues) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P1", 0, 2), "Q0");
    workload.add_process(Process("P2", 6, 3), "Q1");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::idle(2, 6));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 6, 9));
    EXPECT_EQ(res.cpu_idle_time(), 4);
}

TEST(MlqFixedPriorityTest, MultipleProcessesInHighPriorityQueueBeforeLowRuns) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("PLow", 0, 10), "Q1");
    workload.add_process(Process("PH1", 1, 2), "Q0");
    workload.add_process(Process("PH2", 2, 2), "Q0");

    // PLow 0..1 (rem=9)
    // PH1 1..3 (finishes)
    // PH2 3..5 (finishes)
    // PLow 5..14 (finishes)
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("PLow", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("PH1", 1, 3));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("PH2", 3, 5));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("PLow", 5, 14));
}

TEST(MlqFixedPriorityTest, StarvationOfLowerPriorityQueueUntilHighFinishes) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("PLow", 0, 1), "Q1");
    workload.add_process(Process("PH1", 0, 3), "Q0");
    workload.add_process(Process("PH2", 2, 3), "Q0");
    workload.add_process(Process("PH3", 4, 3), "Q0");

    // Q0 has continuous work: PH1 (0..3), PH2 (3..6), PH3 (6..9)
    // PLow starves until time 9!
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("PH1", 0, 3));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("PH2", 3, 6));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("PH3", 6, 9));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("PLow", 9, 10));

    const auto* plow = res.find_process_state("PLow");
    ASSERT_NE(plow, nullptr);
    EXPECT_EQ(plow->waiting_time, 9);
    EXPECT_EQ(plow->response_time, 9);
}

TEST(MlqFixedPriorityTest, SingleQueueFixedPriorityIdenticalToFcfs) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("OnlyQ", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 4), "OnlyQ");
    workload.add_process(Process("P2", 1, 3), "OnlyQ");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 4));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 4, 7));
}

TEST(MlqFixedPriorityTest, MetricIntegrityCpuUtilization) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 2, 4), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_EQ(res.cpu_idle_time(), 2);
    EXPECT_EQ(res.cpu_busy_time(), 4);
    EXPECT_EQ(res.total_execution_time(), 6);
    EXPECT_NEAR(res.cpu_utilization(), (4.0 / 6.0) * 100.0, 1e-6);
}

TEST(MlqFixedPriorityTest, MetricsAverageCalculation) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 4), "Q0"); // CT=4, TAT=4, WT=0, RT=0
    workload.add_process(Process("P2", 0, 6), "Q0"); // CT=10, TAT=10, WT=4, RT=4

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_DOUBLE_EQ(res.average_turnaround_time(), (4.0 + 10.0) / 2.0); // 7.0
    EXPECT_DOUBLE_EQ(res.average_waiting_time(), (0.0 + 4.0) / 2.0);       // 2.0
    EXPECT_DOUBLE_EQ(res.average_response_time(), (0.0 + 4.0) / 2.0);      // 2.0
}

TEST(MlqFixedPriorityTest, UnsortedArrivalTimesNormalized) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P2", 5, 2), "Q0");
    workload.add_process(Process("P1", 0, 3), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::idle(3, 5));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 5, 7));
}

TEST(MlqFixedPriorityTest, EmptyQueueConfiguredDoesNotAffectOtherQueues) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("QEmptyHigh", -10, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("QActive", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 3), "QActive");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 1);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 3));
}

TEST(MlqFixedPriorityTest, MultiPreemptionAndFastReclaim) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P_Low", 0, 10), "Q1");
    workload.add_process(Process("P_High1", 1, 1), "Q0");
    workload.add_process(Process("P_High2", 3, 1), "Q0");

    // P_Low 0..1 (rem=9)
    // P_High1 1..2 (done)
    // P_Low 2..3 (rem=8)
    // P_High2 3..4 (done)
    // P_Low 4..12 (done)
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 5);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P_Low", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P_High1", 1, 2));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P_Low", 2, 3));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P_High2", 3, 4));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P_Low", 4, 12));
}

TEST(MlqFixedPriorityTest, TimelineNormalizationMergesContinuousRuns) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 5), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 1);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));
}

TEST(MlqFixedPriorityTest, QueueIdPreservedInProcessStates) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("SystemQueue", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("UserQueue", 5, QueueAlgorithm::Fcfs));

    workload.add_process(Process("PSys", 0, 2), "SystemQueue");
    workload.add_process(Process("PUsr", 0, 3), "UserQueue");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_EQ(res.find_process_state("PSys")->queue_id, "SystemQueue");
    EXPECT_EQ(res.find_process_state("PUsr")->queue_id, "UserQueue");
}

TEST(MlqFixedPriorityTest, ZeroLengthIntervalsNotRecorded) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 3), "Q0");
    workload.add_process(Process("P2", 3, 2), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // Verify there are no 0-duration segments
    for (const auto& seg : res.timeline()) {
        EXPECT_GT(seg.end_time, seg.start_time);
    }
}

TEST(MlqFixedPriorityTest, TwoQueuesWithMultipleProcessesEach) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("A1", 0, 2), "Q0");
    workload.add_process(Process("A2", 1, 3), "Q0");
    workload.add_process(Process("B1", 0, 4), "Q1");
    workload.add_process(Process("B2", 2, 2), "Q1");

    // Q0 has A1 and A2.
    // At 0: A1 runs 0..2
    // At 2: A2 runs 2..5
    // At 5: Q0 empty. Q1 has B1 (arr 0) and B2 (arr 2). B1 runs 5..9, B2 runs 9..11.
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("A1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("A2", 2, 5));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("B1", 5, 9));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("B2", 9, 11));
}

// ============================================================================
// Group 3: Round Robin Inter-Queue Policy Tests (20 tests)
// ============================================================================

// Scenario C: Round Robin between queues
// Inter-queue quantum: 2
// Q0: FCFS; Q1: FCFS
// P1: AT=0, BT=5, Q0; P2: AT=0, BT=4, Q1
// Expected: P1(0-2) -> P2(2-4) -> P1(4-6) -> P2(6-8) -> P1(8-9)
TEST(MlqRoundRobinTest, ScenarioC_RoundRobinBetweenQueues) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 2;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P1", 0, 5), "Q0");
    workload.add_process(Process("P2", 0, 4), "Q1");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 5);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 6, 8));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 8, 9));

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
    EXPECT_EQ(p2->waiting_time, 4);
    EXPECT_EQ(p2->response_time, 2);
}

// Scenario D: Empty queue skipping
// Inter-queue quantum: 2
// Q0: FCFS; Q1: FCFS
// P1: AT=0, BT=2, Q0; P2: AT=5, BT=3, Q1
// Expected: P1(0-2) -> Idle(2-5) -> P2(5-8)
TEST(MlqRoundRobinTest, ScenarioD_EmptyQueueSkippingAndIdleFastForward) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 2;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P1", 0, 2), "Q0");
    workload.add_process(Process("P2", 5, 3), "Q1");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::idle(2, 5));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 5, 8));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 2);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 8);
    EXPECT_EQ(p2->turnaround_time, 3);
    EXPECT_EQ(p2->waiting_time, 0);
    EXPECT_EQ(p2->response_time, 0);
}

TEST(MlqRoundRobinTest, ThreeQueuesCyclicRoundRobin) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 2;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q2", 2, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P0", 0, 3), "Q0");
    workload.add_process(Process("P1", 0, 3), "Q1");
    workload.add_process(Process("P2", 0, 3), "Q2");

    // Q0 runs 0..2 (P0 rem 1)
    // Q1 runs 2..4 (P1 rem 1)
    // Q2 runs 4..6 (P2 rem 1)
    // Q0 runs 6..7 (P0 finishes)
    // Q1 runs 7..8 (P1 finishes)
    // Q2 runs 8..9 (P2 finishes)
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 6);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P0", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P0", 6, 7));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 7, 8));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P2", 8, 9));
}

TEST(MlqRoundRobinTest, IntermediateEmptyQueueSkippedInCycle) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 2;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs)); // Q1 is empty!
    workload.add_queue(MlqQueueConfig("Q2", 2, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P0", 0, 4), "Q0");
    workload.add_process(Process("P2", 0, 4), "Q2");

    // Q0 runs 0..2 -> Q1 skipped -> Q2 runs 2..4 -> Q0 runs 4..6 -> Q2 runs 6..8
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P0", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P0", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 6, 8));
}

TEST(MlqRoundRobinTest, LateArrivalInSkippedQueueEntersNextCycle) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 2;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P0", 0, 6), "Q0");
    workload.add_process(Process("P1", 3, 2), "Q1"); // arrives at time 3

    // At 0: Q0 runs 0..2 (rem 4). Q1 is empty at 2, so Q0 continues 2..4 (rem 2).
    // At 3: P1 arrives in Q1.
    // At 4: Q0 quantum finishes. Next queue Q1 has P1 ready!
    // Q1 runs 4..6 (P1 finishes).
    // Q0 runs 6..8 (P0 finishes).
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P0", 0, 4));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 4, 6));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P0", 6, 8));
}

TEST(MlqRoundRobinTest, ProcessFinishesBeforeInterQueueQuantumExpires) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 5;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P1", 0, 2), "Q0"); // only needs 2 of the 5 quantum
    workload.add_process(Process("P2", 0, 3), "Q1");

    // Q0 runs P1 0..2 (finishes). Queue Q0 is now empty.
    // Turn switches immediately to Q1.
    // Q1 runs P2 2..5 (finishes).
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 5));
}

TEST(MlqRoundRobinTest, CustomPerQueueInterQueueQuantum) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 10; // default large
    // Q0 gets 1 quantum, Q1 gets 3 quantum
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs, 1, 1));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs, 1, 3));

    workload.add_process(Process("P0", 0, 2), "Q0");
    workload.add_process(Process("P1", 0, 4), "Q1");

    // Q0 runs 0..1 (rem 1)
    // Q1 runs 1..4 (rem 1, used 3)
    // Q0 runs 4..5 (finishes)
    // Q1 runs 5..6 (finishes)
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P0", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 1, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P0", 4, 5));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 5, 6));
}

TEST(MlqRoundRobinTest, InterQueueQuantumOneExtreme) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 1;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P0", 0, 2), "Q0");
    workload.add_process(Process("P1", 0, 2), "Q1");

    // 0..1 P0, 1..2 P1, 2..3 P0, 3..4 P1
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P0", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 1, 2));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P0", 2, 3));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 3, 4));
}

TEST(MlqRoundRobinTest, SingleQueueRoundRobinPreservesWorkload) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 2;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P1", 0, 5), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // Since only Q0 exists, quantum refresh does not switch to other queues, normalized to 1 segment
    ASSERT_EQ(res.timeline().size(), 1);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));
}

TEST(MlqRoundRobinTest, LargeInterQueueQuantumBehavesLikeFcfsBetweenQueues) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 1000;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P1", 0, 5), "Q0");
    workload.add_process(Process("P2", 0, 4), "Q1");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 5, 9));
}

TEST(MlqRoundRobinTest, AllProcessesSimultaneousAcrossFourQueues) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 1;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q2", 2, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q3", 3, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P0", 0, 1), "Q0");
    workload.add_process(Process("P1", 0, 1), "Q1");
    workload.add_process(Process("P2", 0, 1), "Q2");
    workload.add_process(Process("P3", 0, 1), "Q3");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P0", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 1, 2));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 2, 3));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P3", 3, 4));
}

TEST(MlqRoundRobinTest, ReActivationOfQueueAfterBecomingEmpty) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 2;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P0_first", 0, 2), "Q0"); // finishes at 2
    workload.add_process(Process("P1", 0, 6), "Q1");        // runs 2..4, 4..6
    workload.add_process(Process("P0_later", 3, 2), "Q0"); // arrives at 3

    // 0..2 Q0 (P0_first finishes)
    // At 2, Q0 empty, Q1 runs 2..4 (P1 rem 4)
    // At 4, Q0 now has P0_later ready! Turn switches to Q0:
    // Q0 runs 4..6 (P0_later finishes)
    // Q1 runs 6..10 (P1 finishes)
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P0_first", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P0_later", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 6, 10));
}

TEST(MlqRoundRobinTest, MultipleProcessesInsideEachQueueUnderInterQueueRR) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 2;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("A1", 0, 1), "Q0");
    workload.add_process(Process("A2", 0, 2), "Q0");
    workload.add_process(Process("B1", 0, 2), "Q1");

    // Q0 gets 2 slice:
    // A1 runs 0..1 (finishes). Q0 still has 1 unit of inter-queue quantum left!
    // A2 runs 1..2 (rem 1). Inter-queue quantum expired.
    // Q1 gets 2 slice:
    // B1 runs 2..4 (finishes).
    // Q0 gets turn:
    // A2 runs 4..5 (finishes).
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("A1", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("A2", 1, 2));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("B1", 2, 4));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("A2", 4, 5));
}

TEST(MlqRoundRobinTest, MetricResponseTimeMultipleQueues) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 3;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P0", 0, 5), "Q0");
    workload.add_process(Process("P1", 0, 5), "Q1");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    const auto* p0 = res.find_process_state("P0");
    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p0, nullptr);
    ASSERT_NE(p1, nullptr);

    EXPECT_EQ(p0->response_time, 0); // Started at 0
    EXPECT_EQ(p1->response_time, 3); // Started at 3
}

TEST(MlqRoundRobinTest, TwoQueuesAlternatingIdenticalBursts) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 2;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P1", 0, 4), "Q0");
    workload.add_process(Process("P2", 0, 4), "Q1");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 6, 8));
}

TEST(MlqRoundRobinTest, AllProcessesCompletedEndsSimulationCleanly) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 4;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 1), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().total_execution_time(), 1);
}

TEST(MlqRoundRobinTest, WaitingTimeAccountingUnderInterQueueRR) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 2;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P0", 0, 4), "Q0");
    workload.add_process(Process("P1", 0, 2), "Q1");

    // P0: 0..2 (rem 2)
    // P1: 2..4 (finishes, TAT = 4, WT = 4 - 2 = 2)
    // P0: 4..6 (finishes, TAT = 6, WT = 6 - 4 = 2)
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    const auto* p0 = res.find_process_state("P0");
    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p0, nullptr);
    ASSERT_NE(p1, nullptr);

    EXPECT_EQ(p0->waiting_time, 2);
    EXPECT_EQ(p1->waiting_time, 2);
}

TEST(MlqRoundRobinTest, TurnaroundTimeRelationshipHolds) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 2;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P0", 0, 3), "Q0");
    workload.add_process(Process("P1", 1, 3), "Q1");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    for (const auto& st : res.processes()) {
        EXPECT_EQ(st.turnaround_time, st.completion_time - st.arrival_time);
        EXPECT_EQ(st.waiting_time, st.turnaround_time - st.burst_time);
    }
}

TEST(MlqRoundRobinTest, SimultaneousQueueActivationRoundRobin) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 2;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P1", 0, 3), "Q1");
    workload.add_process(Process("P0", 0, 3), "Q0");

    // In Round Robin, initial active queue is configured queue index 0 (Q0)
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P0", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P0", 4, 5));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P1", 5, 6));
}

TEST(MlqRoundRobinTest, LargeWorkloadFairnessCheck) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 10;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    for (int i = 0; i < 5; ++i) {
        workload.add_process(Process("P0_" + std::to_string(i), 0, 10), "Q0");
        workload.add_process(Process("P1_" + std::to_string(i), 0, 10), "Q1");
    }

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    EXPECT_EQ(res.processes().size(), 10);
    EXPECT_EQ(res.cpu_busy_time(), 100);
    EXPECT_EQ(res.cpu_idle_time(), 0);
}

// ============================================================================
// Group 4: Internal Policy Integration Tests (18 tests)
// ============================================================================

// Scenario E: Internal RR and inter-queue scheduling
// Q0: pri 0, RR (internal q=2); Q1: pri 1, FCFS
// Inter-queue: FixedPriorityPreemptive
// P1: AT=0, BT=5, Q0
// P2: AT=0, BT=3, Q0
// P3: AT=0, BT=4, Q1
// Q0 runs first because pri 0 < pri 1.
// Inside Q0: P1 runs 0..2 (rem 3), P2 runs 2..4 (rem 1), P1 runs 4..6 (rem 1), P2 runs 6..7 (done), P1 runs 7..8 (done).
// Then Q1 runs: P3 runs 8..12 (done).
// Timeline: P1(0-2) -> P2(2-4) -> P1(4-6) -> P2(6-7) -> P1(7-8) -> P3(8-12)
TEST(MlqInternalPolicyTest, ScenarioE_InternalRoundRobinWithFixedPriorityInterQueue) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::RoundRobin, 2));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P1", 0, 5), "Q0");
    workload.add_process(Process("P2", 0, 3), "Q0");
    workload.add_process(Process("P3", 0, 4), "Q1");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 6);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 6));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 6, 7));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("P1", 7, 8));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("P3", 8, 12));

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->completion_time, 8);
    EXPECT_EQ(p1->turnaround_time, 8);
    EXPECT_EQ(p1->waiting_time, 3);
    EXPECT_EQ(p1->response_time, 0);

    const auto* p2 = res.find_process_state("P2");
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->completion_time, 7);
    EXPECT_EQ(p2->turnaround_time, 7);
    EXPECT_EQ(p2->waiting_time, 4);
    EXPECT_EQ(p2->response_time, 2);

    const auto* p3 = res.find_process_state("P3");
    ASSERT_NE(p3, nullptr);
    EXPECT_EQ(p3->completion_time, 12);
    EXPECT_EQ(p3->turnaround_time, 12);
    EXPECT_EQ(p3->waiting_time, 8);
    EXPECT_EQ(p3->response_time, 8);
}

TEST(MlqInternalPolicyTest, InternalSjfNonPreemptiveWithinQueue) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("QSjf", 0, QueueAlgorithm::Sjf));

    workload.add_process(Process("P1", 0, 6), "QSjf");
    workload.add_process(Process("P2", 1, 2), "QSjf");
    workload.add_process(Process("P3", 1, 4), "QSjf");

    // At 0: only P1 arrived -> runs 0..6 (non-preemptive).
    // At 6: P2 (BT=2) and P3 (BT=4) ready. SJF selects P2 (BT=2).
    // P2 runs 6..8.
    // P3 runs 8..12.
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 6));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 6, 8));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P3", 8, 12));
}

TEST(MlqInternalPolicyTest, InternalSrtfPreemptiveWithinQueue) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("QSrtf", 0, QueueAlgorithm::Srtf));

    workload.add_process(Process("P1", 0, 8), "QSrtf");
    workload.add_process(Process("P2", 2, 2), "QSrtf");

    // P1 runs 0..2 (rem 6)
    // At 2: P2 arrives with burst 2 < 6 -> intra-queue preemption!
    // P2 runs 2..4 (finishes)
    // P1 resumes 4..10 (finishes)
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 4, 10));
}

TEST(MlqInternalPolicyTest, InternalNonPreemptivePriorityWithinQueue) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("QPri", 0, QueueAlgorithm::NonPreemptivePriority));

    workload.add_process(Process("P1", 0, 5, 10), "QPri");
    workload.add_process(Process("P2", 1, 3, 2), "QPri"); // higher priority (2 < 10)
    workload.add_process(Process("P3", 1, 2, 1), "QPri"); // highest priority (1 < 2)

    // P1 runs non-preemptively 0..5.
    // At 5, both P2 and P3 ready. P3 has priority 1 < 2 -> runs 5..7.
    // Then P2 runs 7..10.
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P3", 5, 7));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P2", 7, 10));
}

TEST(MlqInternalPolicyTest, InternalPreemptivePriorityWithinQueue) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("QPrePri", 0, QueueAlgorithm::PreemptivePriority));

    workload.add_process(Process("P1", 0, 7, 5), "QPrePri");
    workload.add_process(Process("P2", 2, 3, 1), "QPrePri"); // higher priority 1 < 5

    // P1 runs 0..2 (rem 5).
    // At 2, P2 arrives with pri 1 < 5 -> intra-queue preemption!
    // P2 runs 2..5 (finishes).
    // P1 resumes 5..10 (finishes).
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 3);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 2, 5));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 5, 10));
}

TEST(MlqInternalPolicyTest, TwoLevelQuantumInteraction) {
    // Internal RR q=3, inter-queue RR Q=5
    // Q0 has P1(0, 7)
    // Q1 has P2(0, 7)
    // Q0 starts. P1 runs 3 units (0..3) -> internal quantum expires!
    // P1 requeues in Q0. But Q0 still has 2 units of inter-queue quantum left!
    // P1 runs another 2 units (3..5). Now inter-queue quantum expires (used 5).
    // Turn switches to Q1.
    // Normalized, P1 ran continuously 0..5!
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 5;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::RoundRobin, 3));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::RoundRobin, 3));

    workload.add_process(Process("P1", 0, 7), "Q0");
    workload.add_process(Process("P2", 0, 7), "Q1");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    // Q0 runs P1 0..5 (rem 2)
    // Q1 runs P2 5..10 (rem 2)
    // Q0 runs P1 10..12 (finishes)
    // Q1 runs P2 12..14 (finishes)
    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 5, 10));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1", 10, 12));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 12, 14));
}

TEST(MlqInternalPolicyTest, InternalQuantumLargerThanInterQueueQuantum) {
    // Internal RR q=4, inter-queue RR Q=2
    // Q0 has P1(0, 6)
    // Q1 has P2(0, 6)
    // Q0 runs P1 0..2. Inter-queue quantum Q=2 expires. P1 retains rem_burst=4, rem_internal_q=2.
    // Q1 runs P2 2..4.
    // Q0 runs P1 4..6. P1 consumes remaining internal q=2, then gets renewed internal q.
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 2;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::RoundRobin, 4));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::RoundRobin, 4));

    workload.add_process(Process("P1", 0, 6), "Q0");
    workload.add_process(Process("P2", 0, 6), "Q1");

    const auto outcome = MlqScheduler::run(workload);
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

TEST(MlqInternalPolicyTest, SrtfInHighPriorityQueueAndRrInLowPriorityQueue) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("QHigh", 0, QueueAlgorithm::Srtf));
    workload.add_queue(MlqQueueConfig("QLow", 1, QueueAlgorithm::RoundRobin, 2));

    workload.add_process(Process("Low1", 0, 6), "QLow");
    workload.add_process(Process("Low2", 0, 4), "QLow");
    workload.add_process(Process("High1", 1, 4), "QHigh");
    workload.add_process(Process("High2", 2, 1), "QHigh");

    // 0..1: Low1 runs in QLow (rem 5).
    // At 1: High1 arrives in QHigh -> inter-queue preemption! High1 runs 1..2 (rem 3).
    // At 2: High2 arrives in QHigh (BT=1 < 3) -> intra-queue SRTF preemption! High2 runs 2..3 (finishes).
    // High1 resumes 3..6 (finishes).
    // At 6: QHigh is empty. QLow resumes.
    // Low1 had rem internal quantum 1. Low1 runs 6..7 (rem 4), requeues in QLow.
    // Low2 runs 7..9 (rem 2), requeues in QLow.
    // Low1 runs 9..11 (rem 2), requeues in QLow.
    // Low2 runs 11..13 (finishes).
    // Low1 runs 13..15 (finishes).
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 9);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("Low1", 0, 1));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("High1", 1, 2));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("High2", 2, 3));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("High1", 3, 6));
    EXPECT_EQ(res.timeline()[4], ExecutionSegment::process("Low1", 6, 7));
    EXPECT_EQ(res.timeline()[5], ExecutionSegment::process("Low2", 7, 9));
    EXPECT_EQ(res.timeline()[6], ExecutionSegment::process("Low1", 9, 11));
    EXPECT_EQ(res.timeline()[7], ExecutionSegment::process("Low2", 11, 13));
    EXPECT_EQ(res.timeline()[8], ExecutionSegment::process("Low1", 13, 15));
    EXPECT_EQ(res.find_process_state("Low1")->completion_time, 15);
}

TEST(MlqInternalPolicyTest, MixedAlgorithmsAcrossThreeQueues) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Sjf));
    workload.add_queue(MlqQueueConfig("Q2", 2, QueueAlgorithm::RoundRobin, 2));

    workload.add_process(Process("P0", 0, 2), "Q0");
    workload.add_process(Process("P1_long", 0, 5), "Q1");
    workload.add_process(Process("P1_short", 0, 2), "Q1");
    workload.add_process(Process("P2", 0, 2), "Q2");

    // Q0 runs P0 0..2.
    // Q1 has P1_long and P1_short. SJF picks P1_short (2) -> 2..4. Then P1_long (5) -> 4..9.
    // Q2 runs P2 9..11.
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P0", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P1_short", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("P1_long", 4, 9));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("P2", 9, 11));
}

TEST(MlqInternalPolicyTest, SjfTieBreakingByArrivalThenInputOrder) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("QSjf", 0, QueueAlgorithm::Sjf));

    workload.add_process(Process("P1", 0, 3), "QSjf");
    workload.add_process(Process("P2", 0, 3), "QSjf");

    // Both burst 3, arrived at 0. P1 was added first -> P1 then P2
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 3, 6));
}

TEST(MlqInternalPolicyTest, PriorityTieBreakingByArrivalThenInputOrder) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("QPri", 0, QueueAlgorithm::NonPreemptivePriority));

    workload.add_process(Process("P1", 0, 3, 5), "QPri");
    workload.add_process(Process("P2", 0, 3, 5), "QPri");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 3));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 3, 6));
}

TEST(MlqInternalPolicyTest, PreemptivePriorityTieBreakingPreservesRunningProcess) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("QPrePri", 0, QueueAlgorithm::PreemptivePriority));

    workload.add_process(Process("P1", 0, 5, 2), "QPrePri");
    workload.add_process(Process("P2", 2, 2, 2), "QPrePri"); // arrives at 2 with SAME priority 2

    // Preemption only occurs if strictly higher priority (< 2). P2 has equal priority 2, so P1 continues!
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 5, 7));
}

TEST(MlqInternalPolicyTest, SrtfTieBreakingPreservesRunningProcess) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("QSrtf", 0, QueueAlgorithm::Srtf));

    workload.add_process(Process("P1", 0, 6), "QSrtf");
    workload.add_process(Process("P2", 2, 4), "QSrtf"); // At 2, P1 has rem 4. P2 arrives with burst 4.

    // Remaining time is equal (4 == 4). SRTF strictly preempts only when strictly smaller (< 4).
    // So P1 is not preempted and runs to completion!
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 2);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 6));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("P2", 6, 10));
}

TEST(MlqInternalPolicyTest, InternalRoundRobinZeroWaitSingleProcess) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("QRr", 0, QueueAlgorithm::RoundRobin, 2));

    workload.add_process(Process("P1", 0, 5), "QRr");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 1);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->waiting_time, 0);
    EXPECT_EQ(p1->response_time, 0);
    EXPECT_EQ(p1->turnaround_time, 5);
}

TEST(MlqInternalPolicyTest, ResponseTimeTrackedAtFirstExecutionUnderInterQueuePreemption) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("P1", 0, 6), "Q1");
    workload.add_process(Process("P2", 2, 3), "Q0");

    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    const auto* p1 = res.find_process_state("P1");
    ASSERT_NE(p1, nullptr);
    // P1 started at time 0, preempted at 2, resumed at 5
    EXPECT_EQ(p1->response_time, 0);
}

TEST(MlqInternalPolicyTest, InterQueueRoundRobinWithInternalSjf) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::RoundRobin;
    workload.default_inter_queue_quantum = 4;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Sjf));
    workload.add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs));

    workload.add_process(Process("A_long", 0, 6), "Q0");
    workload.add_process(Process("A_short", 0, 2), "Q0");
    workload.add_process(Process("B1", 0, 3), "Q1");

    // Q0 has inter-queue quantum 4. Inside Q0, SJF runs A_short (2) 0..2.
    // Q0 has 2 inter-queue units remaining! Next in Q0 is A_long (6).
    // A_long runs 2..4 (rem 4). Inter-queue quantum expired.
    // Turn switches to Q1:
    // Q1 runs B1 4..7 (finishes).
    // Turn switches to Q0:
    // Q0 runs A_long 7..11 (finishes).
    const auto outcome = MlqScheduler::run(workload);
    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();

    ASSERT_EQ(res.timeline().size(), 4);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("A_short", 0, 2));
    EXPECT_EQ(res.timeline()[1], ExecutionSegment::process("A_long", 2, 4));
    EXPECT_EQ(res.timeline()[2], ExecutionSegment::process("B1", 4, 7));
    EXPECT_EQ(res.timeline()[3], ExecutionSegment::process("A_long", 7, 11));
}

TEST(MlqInternalPolicyTest, InstanceSchedulingMethodMatchesStaticRun) {
    MlqWorkload workload;
    workload.inter_queue_policy = InterQueuePolicy::FixedPriorityPreemptive;
    workload.add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::Fcfs));
    workload.add_process(Process("P1", 0, 3), "Q0");

    MlqScheduler scheduler;
    const auto res1 = scheduler.schedule(workload);
    const auto res2 = MlqScheduler::run(workload);

    ASSERT_TRUE(res1.has_value());
    ASSERT_TRUE(res2.has_value());
    EXPECT_EQ(res1.value().timeline(), res2.value().timeline());
}

