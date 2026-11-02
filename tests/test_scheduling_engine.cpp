#include "cpu_scheduler/scheduling_engine.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace cpu_scheduler;

namespace {

// Standard reference workload
std::vector<Process> create_standard_workload() {
    return {
        Process("P1", 0, 7, 3),
        Process("P2", 2, 4, 1),
        Process("P3", 4, 1, 2),
        Process("P4", 5, 4, 1)
    };
}

// Standard MLQ workload
MlqWorkload create_standard_mlq_workload() {
    MlqWorkload mlq;
    mlq.set_inter_queue_policy(InterQueuePolicy::FixedPriorityPreemptive)
       .add_queue(MlqQueueConfig("Q0", 0, QueueAlgorithm::RoundRobin, 2))
       .add_queue(MlqQueueConfig("Q1", 1, QueueAlgorithm::Fcfs))
       .add_process(Process("P1", 0, 4), "Q0")
       .add_process(Process("P2", 1, 2), "Q0")
       .add_process(Process("P3", 0, 3), "Q1");
    return mlq;
}

// Standard MLFQ config
MlfqConfig create_standard_mlfq_config() {
    return MlfqConfig({2, 4, 8}, 10);
}

} // namespace

// ============================================================================
// Group 1: Introspection and String Conversion Tests
// ============================================================================

TEST(SchedulingEngineIntrospectionTest, ToStringAllSupportedAlgorithms) {
    EXPECT_EQ(to_string(SchedulingAlgorithm::Fcfs), "FCFS");
    EXPECT_EQ(to_string(SchedulingAlgorithm::Sjf), "SJF");
    EXPECT_EQ(to_string(SchedulingAlgorithm::Srtf), "SRTF");
    EXPECT_EQ(to_string(SchedulingAlgorithm::RoundRobin), "RoundRobin");
    EXPECT_EQ(to_string(SchedulingAlgorithm::NonPreemptivePriority), "NonPreemptivePriority");
    EXPECT_EQ(to_string(SchedulingAlgorithm::PreemptivePriority), "PreemptivePriority");
    EXPECT_EQ(to_string(SchedulingAlgorithm::MultilevelQueue), "MLQ");
    EXPECT_EQ(to_string(SchedulingAlgorithm::MultilevelFeedbackQueue), "MLFQ");
    EXPECT_EQ(to_string(static_cast<SchedulingAlgorithm>(999)), "Unknown");
}

TEST(SchedulingEngineIntrospectionTest, AlgorithmNameAllSupportedAlgorithms) {
    EXPECT_EQ(algorithm_name(SchedulingAlgorithm::Fcfs), "First Come, First Served (FCFS)");
    EXPECT_EQ(algorithm_name(SchedulingAlgorithm::Sjf), "Shortest Job First (SJF, Non-Preemptive)");
    EXPECT_EQ(algorithm_name(SchedulingAlgorithm::Srtf), "Shortest Remaining Time First (SRTF, Preemptive)");
    EXPECT_EQ(algorithm_name(SchedulingAlgorithm::RoundRobin), "Round Robin (RR)");
    EXPECT_EQ(algorithm_name(SchedulingAlgorithm::NonPreemptivePriority), "Non-Preemptive Priority");
    EXPECT_EQ(algorithm_name(SchedulingAlgorithm::PreemptivePriority), "Preemptive Priority");
    EXPECT_EQ(algorithm_name(SchedulingAlgorithm::MultilevelQueue), "Multilevel Queue (MLQ)");
    EXPECT_EQ(algorithm_name(SchedulingAlgorithm::MultilevelFeedbackQueue), "Multilevel Feedback Queue (MLFQ)");
    EXPECT_EQ(algorithm_name(static_cast<SchedulingAlgorithm>(999)), "Unknown");
}

TEST(SchedulingEngineIntrospectionTest, AlgorithmFromStringCaseInsensitiveAndAliases) {
    EXPECT_EQ(algorithm_from_string("fcfs"), SchedulingAlgorithm::Fcfs);
    EXPECT_EQ(algorithm_from_string("FCFS"), SchedulingAlgorithm::Fcfs);
    EXPECT_EQ(algorithm_from_string("First Come, First Served"), SchedulingAlgorithm::Fcfs);

    EXPECT_EQ(algorithm_from_string("sjf"), SchedulingAlgorithm::Sjf);
    EXPECT_EQ(algorithm_from_string("Shortest Job First"), SchedulingAlgorithm::Sjf);

    EXPECT_EQ(algorithm_from_string("srtf"), SchedulingAlgorithm::Srtf);
    EXPECT_EQ(algorithm_from_string("preemptive sjf"), SchedulingAlgorithm::Srtf);
    EXPECT_EQ(algorithm_from_string("Shortest Remaining Time First"), SchedulingAlgorithm::Srtf);

    EXPECT_EQ(algorithm_from_string("rr"), SchedulingAlgorithm::RoundRobin);
    EXPECT_EQ(algorithm_from_string("Round Robin"), SchedulingAlgorithm::RoundRobin);

    EXPECT_EQ(algorithm_from_string("nonpreemptivepriority"), SchedulingAlgorithm::NonPreemptivePriority);
    EXPECT_EQ(algorithm_from_string("np_priority"), SchedulingAlgorithm::NonPreemptivePriority);

    EXPECT_EQ(algorithm_from_string("preemptivepriority"), SchedulingAlgorithm::PreemptivePriority);
    EXPECT_EQ(algorithm_from_string("priority"), SchedulingAlgorithm::PreemptivePriority);
    EXPECT_EQ(algorithm_from_string("p_priority"), SchedulingAlgorithm::PreemptivePriority);

    EXPECT_EQ(algorithm_from_string("mlq"), SchedulingAlgorithm::MultilevelQueue);
    EXPECT_EQ(algorithm_from_string("multilevel queue"), SchedulingAlgorithm::MultilevelQueue);

    EXPECT_EQ(algorithm_from_string("mlfq"), SchedulingAlgorithm::MultilevelFeedbackQueue);
    EXPECT_EQ(algorithm_from_string("multilevel feedback queue"), SchedulingAlgorithm::MultilevelFeedbackQueue);

    EXPECT_EQ(algorithm_from_string("nonexistent"), std::nullopt);
    EXPECT_EQ(algorithm_from_string(""), std::nullopt);
}

TEST(SchedulingEngineIntrospectionTest, SupportedAlgorithmsSpanContainsAllEight) {
    const auto algos = supported_algorithms();
    ASSERT_EQ(algos.size(), 8);
    EXPECT_EQ(algos[0], SchedulingAlgorithm::Fcfs);
    EXPECT_EQ(algos[1], SchedulingAlgorithm::Sjf);
    EXPECT_EQ(algos[2], SchedulingAlgorithm::Srtf);
    EXPECT_EQ(algos[3], SchedulingAlgorithm::RoundRobin);
    EXPECT_EQ(algos[4], SchedulingAlgorithm::NonPreemptivePriority);
    EXPECT_EQ(algos[5], SchedulingAlgorithm::PreemptivePriority);
    EXPECT_EQ(algos[6], SchedulingAlgorithm::MultilevelQueue);
    EXPECT_EQ(algos[7], SchedulingAlgorithm::MultilevelFeedbackQueue);
}

// ============================================================================
// Group 2: Algorithm Dispatch Parity Tests (Matching Direct Invocations)
// ============================================================================

TEST(SchedulingEngineDispatchTest, FcfsParityWithDirectScheduler) {
    const auto workload = create_standard_workload();
    const auto direct = FcfsScheduler::run(workload);
    const auto unified = SchedulingEngine::execute(SchedulingRequest::fcfs(workload));

    ASSERT_TRUE(direct.has_value());
    ASSERT_TRUE(unified.has_value());
    EXPECT_EQ(unified.value().timeline(), direct.value().timeline());
    EXPECT_EQ(unified.value().processes(), direct.value().processes());
    EXPECT_DOUBLE_EQ(unified.value().average_waiting_time(), direct.value().average_waiting_time());
    EXPECT_DOUBLE_EQ(unified.value().average_turnaround_time(), direct.value().average_turnaround_time());
    EXPECT_DOUBLE_EQ(unified.value().cpu_utilization(), direct.value().cpu_utilization());
}

TEST(SchedulingEngineDispatchTest, SjfParityWithDirectScheduler) {
    const auto workload = create_standard_workload();
    const auto direct = SjfScheduler::run(workload);
    const auto unified = SchedulingEngine::execute(SchedulingRequest::sjf(workload));

    ASSERT_TRUE(direct.has_value());
    ASSERT_TRUE(unified.has_value());
    EXPECT_EQ(unified.value().timeline(), direct.value().timeline());
    EXPECT_EQ(unified.value().processes(), direct.value().processes());
    EXPECT_DOUBLE_EQ(unified.value().average_waiting_time(), direct.value().average_waiting_time());
    EXPECT_DOUBLE_EQ(unified.value().average_turnaround_time(), direct.value().average_turnaround_time());
}

TEST(SchedulingEngineDispatchTest, SrtfParityWithDirectScheduler) {
    const auto workload = create_standard_workload();
    const auto direct = SrtfScheduler::run(workload);
    const auto unified = SchedulingEngine::execute(SchedulingRequest::srtf(workload));

    ASSERT_TRUE(direct.has_value());
    ASSERT_TRUE(unified.has_value());
    EXPECT_EQ(unified.value().timeline(), direct.value().timeline());
    EXPECT_EQ(unified.value().processes(), direct.value().processes());
    EXPECT_DOUBLE_EQ(unified.value().average_waiting_time(), direct.value().average_waiting_time());
    EXPECT_DOUBLE_EQ(unified.value().average_turnaround_time(), direct.value().average_turnaround_time());
}

TEST(SchedulingEngineDispatchTest, RoundRobinParityWithDirectScheduler) {
    const auto workload = create_standard_workload();
    constexpr Time quantum = 2;
    const auto direct = RrScheduler::run(workload, quantum);
    const auto unified = SchedulingEngine::execute(SchedulingRequest::round_robin(workload, quantum));

    ASSERT_TRUE(direct.has_value());
    ASSERT_TRUE(unified.has_value());
    EXPECT_EQ(unified.value().timeline(), direct.value().timeline());
    EXPECT_EQ(unified.value().processes(), direct.value().processes());
    EXPECT_DOUBLE_EQ(unified.value().average_waiting_time(), direct.value().average_waiting_time());
    EXPECT_DOUBLE_EQ(unified.value().average_turnaround_time(), direct.value().average_turnaround_time());
}

TEST(SchedulingEngineDispatchTest, NonPreemptivePriorityParityWithDirectScheduler) {
    const auto workload = create_standard_workload();
    const auto direct = NonPreemptivePriorityScheduler::run(workload);
    const auto unified = SchedulingEngine::execute(SchedulingRequest::non_preemptive_priority(workload));

    ASSERT_TRUE(direct.has_value());
    ASSERT_TRUE(unified.has_value());
    EXPECT_EQ(unified.value().timeline(), direct.value().timeline());
    EXPECT_EQ(unified.value().processes(), direct.value().processes());
    EXPECT_DOUBLE_EQ(unified.value().average_waiting_time(), direct.value().average_waiting_time());
    EXPECT_DOUBLE_EQ(unified.value().average_turnaround_time(), direct.value().average_turnaround_time());
}

TEST(SchedulingEngineDispatchTest, PreemptivePriorityParityWithDirectScheduler) {
    const auto workload = create_standard_workload();
    const auto direct = PreemptivePriorityScheduler::run(workload);
    const auto unified = SchedulingEngine::execute(SchedulingRequest::preemptive_priority(workload));

    ASSERT_TRUE(direct.has_value());
    ASSERT_TRUE(unified.has_value());
    EXPECT_EQ(unified.value().timeline(), direct.value().timeline());
    EXPECT_EQ(unified.value().processes(), direct.value().processes());
    EXPECT_DOUBLE_EQ(unified.value().average_waiting_time(), direct.value().average_waiting_time());
    EXPECT_DOUBLE_EQ(unified.value().average_turnaround_time(), direct.value().average_turnaround_time());
}

TEST(SchedulingEngineDispatchTest, MultilevelQueueParityWithDirectScheduler) {
    const auto mlq_workload = create_standard_mlq_workload();
    const auto direct = MlqScheduler::run(mlq_workload);
    const auto unified = SchedulingEngine::execute(SchedulingRequest::mlq(mlq_workload));

    ASSERT_TRUE(direct.has_value());
    ASSERT_TRUE(unified.has_value());
    EXPECT_EQ(unified.value().timeline(), direct.value().timeline());
    EXPECT_EQ(unified.value().processes(), direct.value().processes());
    EXPECT_DOUBLE_EQ(unified.value().average_waiting_time(), direct.value().average_waiting_time());
    EXPECT_DOUBLE_EQ(unified.value().average_turnaround_time(), direct.value().average_turnaround_time());
}

TEST(SchedulingEngineDispatchTest, MultilevelFeedbackQueueParityWithDirectScheduler) {
    const auto workload = create_standard_workload();
    const auto mlfq_config = create_standard_mlfq_config();
    const auto direct = MlfqScheduler::run(workload, mlfq_config);
    const auto unified = SchedulingEngine::execute(SchedulingRequest::mlfq(workload, mlfq_config));

    ASSERT_TRUE(direct.has_value());
    ASSERT_TRUE(unified.has_value());
    EXPECT_EQ(unified.value().timeline(), direct.value().timeline());
    EXPECT_EQ(unified.value().processes(), direct.value().processes());
    EXPECT_DOUBLE_EQ(unified.value().average_waiting_time(), direct.value().average_waiting_time());
    EXPECT_DOUBLE_EQ(unified.value().average_turnaround_time(), direct.value().average_turnaround_time());
}

// ============================================================================
// Group 3: Distinction Between Preemptive and Non-Preemptive Variants
// ============================================================================

TEST(SchedulingEngineDispatchTest, SjfAndSrtfProduceDistinctSchedules) {
    const auto workload = create_standard_workload();
    const auto sjf_outcome = SchedulingEngine::execute(SchedulingRequest::sjf(workload));
    const auto srtf_outcome = SchedulingEngine::execute(SchedulingRequest::srtf(workload));

    ASSERT_TRUE(sjf_outcome.has_value());
    ASSERT_TRUE(srtf_outcome.has_value());

    // In standard workload, P2 preempts P1 in SRTF at t=2, but does not in SJF
    EXPECT_NE(sjf_outcome.value().timeline(), srtf_outcome.value().timeline());
    EXPECT_NE(sjf_outcome.value().average_waiting_time(), srtf_outcome.value().average_waiting_time());
}

TEST(SchedulingEngineDispatchTest, NonPreemptiveAndPreemptivePriorityProduceDistinctSchedules) {
    const auto workload = create_standard_workload();
    const auto np_outcome = SchedulingEngine::execute(SchedulingRequest::non_preemptive_priority(workload));
    const auto p_outcome = SchedulingEngine::execute(SchedulingRequest::preemptive_priority(workload));

    ASSERT_TRUE(np_outcome.has_value());
    ASSERT_TRUE(p_outcome.has_value());

    // In standard workload, P2 (priority 1) preempts P1 (priority 3) in preemptive priority
    EXPECT_NE(np_outcome.value().timeline(), p_outcome.value().timeline());
    EXPECT_NE(np_outcome.value().average_waiting_time(), p_outcome.value().average_waiting_time());
}

// ============================================================================
// Group 4: Configuration Validation Tests
// ============================================================================

TEST(SchedulingEngineConfigTest, MissingRoundRobinQuantumRejected) {
    SchedulingRequest req;
    req.algorithm = SchedulingAlgorithm::RoundRobin;
    req.workload = { Process("P1", 0, 5) };
    req.config = std::monostate{}; // Missing config

    const auto outcome = SchedulingEngine::execute(req);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("missing configuration for Round Robin"), std::string::npos);
}

TEST(SchedulingEngineConfigTest, ZeroRoundRobinQuantumRejected) {
    SchedulingRequest req;
    req.algorithm = SchedulingAlgorithm::RoundRobin;
    req.workload = { Process("P1", 0, 5) };
    req.config = RoundRobinConfig{0}; // Zero quantum

    const auto outcome = SchedulingEngine::execute(req);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("positive (> 0)"), std::string::npos);
}

TEST(SchedulingEngineConfigTest, NegativeRoundRobinQuantumRejected) {
    SchedulingRequest req;
    req.algorithm = SchedulingAlgorithm::RoundRobin;
    req.workload = { Process("P1", 0, 5) };
    req.config = RoundRobinConfig{-5}; // Negative quantum

    const auto outcome = SchedulingEngine::execute(req);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("positive (> 0)"), std::string::npos);
}

TEST(SchedulingEngineConfigTest, IncompatibleConfigForRoundRobinRejected) {
    SchedulingRequest req;
    req.algorithm = SchedulingAlgorithm::RoundRobin;
    req.workload = { Process("P1", 0, 5) };
    req.config = MlfqConfig({2, 4}); // Incompatible config type

    const auto outcome = SchedulingEngine::execute(req);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("incompatible configuration type for Round Robin"), std::string::npos);
}

TEST(SchedulingEngineConfigTest, MissingMlqWorkloadRejected) {
    SchedulingRequest req;
    req.algorithm = SchedulingAlgorithm::MultilevelQueue;
    req.workload = { Process("P1", 0, 5) };
    req.config = std::monostate{}; // Missing config

    const auto outcome = SchedulingEngine::execute(req);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("missing configuration for Multilevel Queue"), std::string::npos);
}

TEST(SchedulingEngineConfigTest, IncompatibleConfigForMlqRejected) {
    SchedulingRequest req;
    req.algorithm = SchedulingAlgorithm::MultilevelQueue;
    req.workload = { Process("P1", 0, 5) };
    req.config = RoundRobinConfig{2}; // Incompatible config type

    const auto outcome = SchedulingEngine::execute(req);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("incompatible configuration type for Multilevel Queue"), std::string::npos);
}

TEST(SchedulingEngineConfigTest, InvalidMlqWorkloadDelegatedAndRejected) {
    MlqWorkload empty_mlq; // No queues configured
    const auto outcome = SchedulingEngine::execute(SchedulingRequest::mlq(empty_mlq));

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("at least one queue"), std::string::npos);
}

TEST(SchedulingEngineConfigTest, MissingMlfqConfigRejected) {
    SchedulingRequest req;
    req.algorithm = SchedulingAlgorithm::MultilevelFeedbackQueue;
    req.workload = { Process("P1", 0, 5) };
    req.config = std::monostate{}; // Missing config

    const auto outcome = SchedulingEngine::execute(req);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("missing configuration for Multilevel Feedback Queue"), std::string::npos);
}

TEST(SchedulingEngineConfigTest, IncompatibleConfigForMlfqRejected) {
    SchedulingRequest req;
    req.algorithm = SchedulingAlgorithm::MultilevelFeedbackQueue;
    req.workload = { Process("P1", 0, 5) };
    req.config = RoundRobinConfig{2}; // Incompatible config type

    const auto outcome = SchedulingEngine::execute(req);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("incompatible configuration type for Multilevel Feedback Queue"), std::string::npos);
}

TEST(SchedulingEngineConfigTest, InvalidMlfqLevelsDelegatedAndRejected) {
    MlfqConfig invalid_mlfq; // Less than 2 levels
    invalid_mlfq.add_level(2);
    const auto outcome = SchedulingEngine::execute(
        SchedulingRequest::mlfq({ Process("P1", 0, 5) }, invalid_mlfq));

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("at least two queue levels"), std::string::npos);
}

TEST(SchedulingEngineConfigTest, IrrelevantConfigForFcfsRejected) {
    SchedulingRequest req;
    req.algorithm = SchedulingAlgorithm::Fcfs;
    req.workload = { Process("P1", 0, 5) };
    req.config = RoundRobinConfig{2}; // Irrelevant config

    const auto outcome = SchedulingEngine::execute(req);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("irrelevant configuration supplied"), std::string::npos);
}

TEST(SchedulingEngineConfigTest, IrrelevantConfigForSjfRejected) {
    SchedulingRequest req;
    req.algorithm = SchedulingAlgorithm::Sjf;
    req.workload = { Process("P1", 0, 5) };
    req.config = RoundRobinConfig{2}; // Irrelevant config

    const auto outcome = SchedulingEngine::execute(req);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("irrelevant configuration supplied"), std::string::npos);
}

TEST(SchedulingEngineConfigTest, IrrelevantConfigForSrtfRejected) {
    SchedulingRequest req;
    req.algorithm = SchedulingAlgorithm::Srtf;
    req.workload = { Process("P1", 0, 5) };
    req.config = MlfqConfig({2, 4}); // Irrelevant config

    const auto outcome = SchedulingEngine::execute(req);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("irrelevant configuration supplied"), std::string::npos);
}

TEST(SchedulingEngineConfigTest, IrrelevantConfigForPriorityRejected) {
    SchedulingRequest req;
    req.algorithm = SchedulingAlgorithm::NonPreemptivePriority;
    req.workload = { Process("P1", 0, 5) };
    req.config = RoundRobinConfig{4}; // Irrelevant config

    const auto outcome = SchedulingEngine::execute(req);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("irrelevant configuration supplied"), std::string::npos);
}

TEST(SchedulingEngineConfigTest, InvalidAlgorithmEnumIdentifierRejected) {
    SchedulingRequest req;
    req.algorithm = static_cast<SchedulingAlgorithm>(42);
    req.workload = { Process("P1", 0, 5) };

    const auto outcome = SchedulingEngine::execute(req);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    EXPECT_NE(outcome.error().message.find("unknown or unsupported scheduling algorithm"), std::string::npos);
}

// ============================================================================
// Group 5: Workload Edge Cases
// ============================================================================

TEST(SchedulingEngineWorkloadTest, EmptyWorkloadRejectedAcrossAlgorithms) {
    const std::vector<Process> empty_workload{};

    const auto fcfs_res = SchedulingEngine::execute(SchedulingRequest::fcfs(empty_workload));
    ASSERT_FALSE(fcfs_res.has_value());
    EXPECT_EQ(fcfs_res.error().kind, ScheduleErrorKind::ValidationError);

    const auto rr_res = SchedulingEngine::execute(SchedulingRequest::round_robin(empty_workload, 2));
    ASSERT_FALSE(rr_res.has_value());
    EXPECT_EQ(rr_res.error().kind, ScheduleErrorKind::ValidationError);

    const auto mlfq_res = SchedulingEngine::execute(SchedulingRequest::mlfq(empty_workload, create_standard_mlfq_config()));
    ASSERT_FALSE(mlfq_res.has_value());
    EXPECT_EQ(mlfq_res.error().kind, ScheduleErrorKind::ValidationError);
}

TEST(SchedulingEngineWorkloadTest, SingleProcessExecutionSucceeds) {
    const std::vector<Process> single = { Process("P1", 0, 5) };
    const auto outcome = SchedulingEngine::execute(SchedulingRequest::fcfs(single));

    ASSERT_TRUE(outcome.has_value());
    const auto& res = outcome.value();
    EXPECT_EQ(res.timeline().size(), 1);
    EXPECT_EQ(res.timeline()[0], ExecutionSegment::process("P1", 0, 5));
    EXPECT_EQ(res.processes().size(), 1);
    EXPECT_EQ(res.processes()[0].turnaround_time, 5);
    EXPECT_EQ(res.processes()[0].waiting_time, 0);
}

TEST(SchedulingEngineWorkloadTest, UnsortedArrivalTimesNormalizedCorrectly) {
    const std::vector<Process> unsorted = {
        Process("P3", 4, 2),
        Process("P1", 0, 3),
        Process("P2", 1, 1)
    };
    const auto outcome = SchedulingEngine::execute(SchedulingRequest::fcfs(unsorted));

    ASSERT_TRUE(outcome.has_value());
    const auto& timeline = outcome.value().timeline();
    ASSERT_EQ(timeline.size(), 3);
    EXPECT_EQ(timeline[0].process_id, "P1");
    EXPECT_EQ(timeline[1].process_id, "P2");
    EXPECT_EQ(timeline[2].process_id, "P3");
}

TEST(SchedulingEngineWorkloadTest, SimultaneousArrivalsPreserveOriginalInputOrder) {
    const std::vector<Process> simultaneous = {
        Process("P_first", 0, 2),
        Process("P_second", 0, 2),
        Process("P_third", 0, 2)
    };
    const auto outcome = SchedulingEngine::execute(SchedulingRequest::fcfs(simultaneous));

    ASSERT_TRUE(outcome.has_value());
    const auto& timeline = outcome.value().timeline();
    ASSERT_EQ(timeline.size(), 3);
    EXPECT_EQ(timeline[0].process_id, "P_first");
    EXPECT_EQ(timeline[1].process_id, "P_second");
    EXPECT_EQ(timeline[2].process_id, "P_third");
}

TEST(SchedulingEngineWorkloadTest, InvalidProcessDataRejectedWithValidationResult) {
    const std::vector<Process> invalid_workload = {
        Process("P1", -1, 5) // Negative arrival time
    };
    const auto outcome = SchedulingEngine::execute(SchedulingRequest::fcfs(invalid_workload));

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().kind, ScheduleErrorKind::ValidationError);
    ASSERT_TRUE(outcome.error().validation_result.has_value());
    EXPECT_FALSE(outcome.error().validation_result->is_valid());
}

TEST(SchedulingEngineWorkloadTest, InitialIdlePeriodRecordedAccurately) {
    const std::vector<Process> delayed = { Process("P1", 5, 3) };
    const auto outcome = SchedulingEngine::execute(SchedulingRequest::fcfs(delayed));

    ASSERT_TRUE(outcome.has_value());
    const auto& timeline = outcome.value().timeline();
    ASSERT_EQ(timeline.size(), 2);
    EXPECT_TRUE(timeline[0].is_idle());
    EXPECT_EQ(timeline[0].duration(), 5);
    EXPECT_TRUE(timeline[1].is_process());
    EXPECT_EQ(timeline[1].duration(), 3);
    EXPECT_EQ(outcome.value().cpu_idle_time(), 5);
    EXPECT_EQ(outcome.value().cpu_busy_time(), 3);
}

// ============================================================================
// Group 6: Fluent Builders and Convenience Overloads
// ============================================================================

TEST(SchedulingEngineConvenienceTest, FluentBuilderExecution) {
    SchedulingRequest req;
    req.set_algorithm(SchedulingAlgorithm::RoundRobin)
       .set_workload({ Process("P1", 0, 4), Process("P2", 1, 2) })
       .set_config(RoundRobinConfig{2});

    const auto outcome = SchedulingEngine::schedule(req);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().processes().size(), 2);
}

TEST(SchedulingEngineConvenienceTest, ScheduleConvenienceOverloadSpan) {
    const std::vector<Process> workload = { Process("P1", 0, 3), Process("P2", 1, 2) };
    const auto outcome = SchedulingEngine::schedule(
        SchedulingAlgorithm::Sjf,
        std::span<const Process>(workload));

    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().processes().size(), 2);
}

TEST(SchedulingEngineConvenienceTest, ScheduleConvenienceOverloadVector) {
    const std::vector<Process> workload = { Process("P1", 0, 3), Process("P2", 1, 2) };
    const auto outcome = SchedulingEngine::schedule(
        SchedulingAlgorithm::Srtf,
        workload);

    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().processes().size(), 2);
}

TEST(SchedulingEngineConvenienceTest, UnifiedSchedulerTypeAliasMatchesSchedulingEngine) {
    const std::vector<Process> workload = { Process("P1", 0, 3) };
    const auto outcome = UnifiedScheduler::schedule(SchedulingRequest::fcfs(workload));

    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome.value().processes()[0].process_id, "P1");
}
