#include "cpu_scheduler/cli.hpp"
#include "cpu_scheduler/scheduling_engine.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

namespace cpu_scheduler {
namespace {

TEST(CliTest, RunWithOption9ExitsGracefully) {
    std::istringstream input("9\n");
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    EXPECT_NE(output.str().find("CPU SCHEDULING SIMULATOR"), std::string::npos);
    EXPECT_NE(output.str().find("Thank you for using the CPU Scheduling Simulator."), std::string::npos);
}

TEST(CliTest, RunWithImmediateEofExitsGracefully) {
    std::istringstream input("");
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    EXPECT_NE(output.str().find("Thank you for using the CPU Scheduling Simulator."), std::string::npos);
}

TEST(CliTest, MenuInvalidSelectionRetriesAndExits) {
    std::istringstream input("abc\n0\n10\n-1\n9\n");
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    EXPECT_NE(output.str().find("Error: Invalid input."), std::string::npos);
}

TEST(CliTest, FcfsEndToEndExecution) {
    // Menu: 1 (FCFS)
    // Procs: 2
    // P1: arr 0, burst 3
    // P2: arr 2, burst 4
    // Post-sim: 2 (Exit)
    std::string in_str =
        "1\n"       // FCFS
        "2\n"       // 2 processes
        "P1\n0\n3\n"
        "P2\n2\n4\n"
        "2\n";      // Exit post simulation

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("SIMULATION SUMMARY"), std::string::npos);
    EXPECT_NE(out.find("First Come, First Served"), std::string::npos);
    EXPECT_NE(out.find("P1"), std::string::npos);
    EXPECT_NE(out.find("P2"), std::string::npos);
    EXPECT_NE(out.find("Execution Timeline:"), std::string::npos);
    EXPECT_NE(out.find("[0 - 3: P1] [3 - 7: P2]"), std::string::npos);
}

TEST(CliTest, SjfEndToEndExecution) {
    // Menu: 2 (SJF)
    // Procs: 2
    // P1: arr 0, burst 5
    // P2: arr 1, burst 2
    // Post-sim: 2 (Exit)
    std::string in_str =
        "2\n"       // SJF
        "2\n"
        "P1\n0\n5\n"
        "P2\n1\n2\n"
        "2\n";

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Shortest Job First"), std::string::npos);
    EXPECT_NE(out.find("[0 - 5: P1] [5 - 7: P2]"), std::string::npos);
}

TEST(CliTest, SrtfEndToEndExecution) {
    // Menu: 3 (SRTF)
    // Procs: 2
    // P1: arr 0, burst 6
    // P2: arr 2, burst 2
    // Post-sim: 2 (Exit)
    std::string in_str =
        "3\n"       // SRTF
        "2\n"
        "P1\n0\n6\n"
        "P2\n2\n2\n"
        "2\n";

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Shortest Remaining Time First"), std::string::npos);
    EXPECT_NE(out.find("[0 - 2: P1] [2 - 4: P2] [4 - 8: P1]"), std::string::npos);
}

TEST(CliTest, RoundRobinEndToEndExecution) {
    // Menu: 4 (RR)
    // Quantum: 2
    // Procs: 2
    // P1: arr 0, burst 3
    // P2: arr 1, burst 3
    // Post-sim: 2 (Exit)
    std::string in_str =
        "4\n"       // RR
        "2\n"       // Quantum = 2
        "2\n"       // 2 processes
        "P1\n0\n3\n"
        "P2\n1\n3\n"
        "2\n";

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Round Robin"), std::string::npos);
    EXPECT_NE(out.find("[0 - 2: P1] [2 - 4: P2] [4 - 5: P1] [5 - 6: P2]"), std::string::npos);
}

TEST(CliTest, NonPreemptivePriorityEndToEndExecution) {
    // Menu: 5 (Priority NP)
    // Procs: 2
    // P1: arr 0, burst 4, prio 2
    // P2: arr 1, burst 3, prio 1
    // Post-sim: 2 (Exit)
    std::string in_str =
        "5\n"       // Non-preemptive priority
        "2\n"       // 2 processes
        "P1\n0\n4\n2\n"
        "P2\n1\n3\n1\n"
        "2\n";

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Non-Preemptive Priority"), std::string::npos);
    EXPECT_NE(out.find("[0 - 4: P1] [4 - 7: P2]"), std::string::npos);
}

TEST(CliTest, PreemptivePriorityEndToEndExecution) {
    // Menu: 6 (Priority Preemptive)
    // Procs: 2
    // P1: arr 0, burst 6, prio 2
    // P2: arr 2, burst 2, prio 1
    // Post-sim: 2 (Exit)
    std::string in_str =
        "6\n"       // Preemptive priority
        "2\n"       // 2 processes
        "P1\n0\n6\n2\n"
        "P2\n2\n2\n1\n"
        "2\n";

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Preemptive Priority"), std::string::npos);
    EXPECT_NE(out.find("[0 - 2: P1] [2 - 4: P2] [4 - 8: P1]"), std::string::npos);
}

TEST(CliTest, MlqEndToEndExecution) {
    // Menu: 7 (MLQ)
    // Queues: 2
    // Q1: prio 0, algo 1 (FCFS)
    // Q2: prio 1, algo 1 (FCFS)
    // Inter-queue policy: 1 (FixedPriorityPreemptive)
    // Procs: 2
    // P1: arr 0, burst 3, prio 0, queue Q1
    // P2: arr 1, burst 4, prio 0, queue Q2
    // Post-sim: 2 (Exit)
    std::string in_str =
        "7\n"       // MLQ
        "2\n"       // 2 queues
        "Q1\n0\n1\n"  // Q1, priority 0, FCFS
        "Q2\n1\n1\n"  // Q2, priority 1, FCFS
        "1\n"         // Fixed Priority Preemptive
        "2\n"         // 2 processes
        "P1\n0\n3\n0\nQ1\n"
        "P2\n1\n4\n0\nQ2\n"
        "2\n";

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Multilevel Queue"), std::string::npos);
    EXPECT_NE(out.find("Q1"), std::string::npos);
    EXPECT_NE(out.find("Q2"), std::string::npos);
    EXPECT_NE(out.find("[0 - 3: P1] [3 - 7: P2]"), std::string::npos);
}

TEST(CliTest, MlfqEndToEndExecution) {
    // Menu: 8 (MLFQ)
    // Levels: 2
    // Level 0: quantum 2, custom allotment: n
    // Level 1: quantum 4, custom allotment: n
    // Boost: n
    // Procs: 1
    // P1: arr 0, burst 5
    // Post-sim: 2 (Exit)
    std::string in_str =
        "8\n"       // MLFQ
        "2\n"       // 2 levels
        "2\nn\n"    // level 0: quantum 2, no custom allotment
        "4\nn\n"    // level 1: quantum 4, no custom allotment
        "n\n"       // no boost
        "1\n"       // 1 process
        "P1\n0\n5\n"
        "2\n";

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Multilevel Feedback Queue"), std::string::npos);
    EXPECT_NE(out.find("P1"), std::string::npos);
    EXPECT_NE(out.find("[0 - 2: P1] [2 - 5: P1]"), std::string::npos);
}

TEST(CliTest, MultipleSimulationsInOneSession) {
    // 1st sim: FCFS with 1 process
    // Post-sim: 1 (Run another)
    // 2nd sim: SJF with 1 process
    // Post-sim: 2 (Exit)
    std::string in_str =
        "1\n"       // FCFS
        "1\n"
        "P1\n0\n3\n"
        "1\n"       // Run another simulation
        "2\n"       // SJF
        "1\n"
        "P2\n0\n4\n"
        "2\n";      // Exit

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("First Come, First Served"), std::string::npos);
    EXPECT_NE(out.find("Shortest Job First"), std::string::npos);
    EXPECT_NE(out.find("[0 - 3: P1]"), std::string::npos);
    EXPECT_NE(out.find("[0 - 4: P2]"), std::string::npos);
}

TEST(CliTest, WorkloadValidationInvalidProcessCount) {
    // Process count: -1 -> 0 -> 101 -> foo -> 1
    std::string in_str =
        "1\n"
        "-1\n"
        "0\n"
        "101\n"
        "foo\n"
        "1\n"
        "P1\n0\n2\n"
        "2\n";

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Error: Invalid input."), std::string::npos);
}

TEST(CliTest, WorkloadValidationDuplicateProcessId) {
    // Process count: 2
    // Proc 1: P1, 0, 2
    // Proc 2: P1 (rejected) -> p1 (rejected) -> P2, 0, 2
    std::string in_str =
        "1\n"
        "2\n"
        "P1\n0\n2\n"
        "P1\n"       // duplicate
        "p1\n"       // case-insensitive duplicate
        "P2\n0\n2\n"
        "2\n";

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Error: Process ID 'P1' already exists."), std::string::npos);
    EXPECT_NE(out.find("Error: Process ID 'p1' already exists."), std::string::npos);
}

TEST(CliTest, WorkloadValidationNegativeArrivalAndBurst) {
    // Proc 1: arrival -5 -> 0, burst 0 -> -2 -> 3
    std::string in_str =
        "1\n"
        "1\n"
        "P1\n"
        "-5\n"   // negative arrival rejected
        "0\n"    // valid arrival
        "0\n"    // zero burst rejected
        "-2\n"   // negative burst rejected
        "3\n"    // valid burst
        "2\n";

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Error: Invalid input."), std::string::npos);
    EXPECT_NE(out.find("[0 - 3: P1]"), std::string::npos);
}

TEST(CliTest, RoundRobinInvalidQuantumRejection) {
    // Quantum: 0 -> -3 -> bar -> 3
    std::string in_str =
        "4\n"
        "0\n"
        "-3\n"
        "bar\n"
        "3\n"
        "1\n"
        "P1\n0\n4\n"
        "2\n";

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Error: Invalid input."), std::string::npos);
}

TEST(CliTest, MlqInvalidQueueBindingAndDuplicateQueueId) {
    // 2 queues: Q1 then Q1 (duplicate) -> Q2
    // Process binding: Q3 (unknown) -> Q1
    std::string in_str =
        "7\n"
        "2\n"
        "Q1\n0\n1\n"
        "Q1\n"        // duplicate queue ID
        "Q2\n1\n1\n"
        "1\n"         // Fixed Priority Preemptive
        "1\n"         // 1 process
        "P1\n0\n2\n0\n"
        "Q3\n"        // Non-existent queue
        "Q1\n"        // Valid queue
        "2\n";

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Error: Queue ID 'Q1' already exists."), std::string::npos);
    EXPECT_NE(out.find("Error: Queue 'Q3' not found."), std::string::npos);
}

TEST(CliTest, MlfqCustomAllotmentAndBoostEnabled) {
    // MLFQ with 2 levels:
    // Level 0: quantum 2, custom allotment: y, allot 4
    // Level 1: quantum 6, custom allotment: n
    // Boost: y, interval 10
    // 1 process: P1 arr 0, burst 3
    std::string in_str =
        "8\n"
        "2\n"
        "2\ny\n4\n"
        "6\nn\n"
        "y\n10\n"
        "1\n"
        "P1\n0\n3\n"
        "2\n";

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Multilevel Feedback Queue"), std::string::npos);
    EXPECT_NE(out.find("[0 - 2: P1] [2 - 3: P1]"), std::string::npos);
}

TEST(CliTest, FormatErrorDisplaysMessageAndValidationDetails) {
    ScheduleError err;
    err.kind = ScheduleErrorKind::ValidationError;
    err.message = "Workload validation failed";
    err.validation_result = ValidationResult({
        ValidationError{ValidationErrorCode::DuplicateProcessId, "Duplicate process ID 'P1'", "P1", 1}
    });

    std::ostringstream out;
    CliController::format_error(out, err);

    std::string str = out.str();
    EXPECT_NE(str.find("SIMULATION FAILED"), std::string::npos);
    EXPECT_NE(str.find("Workload validation failed"), std::string::npos);
    EXPECT_NE(str.find("Duplicate process ID 'P1'"), std::string::npos);
}

TEST(CliTest, FormatTimelineIdlePeriod) {
    std::vector<ExecutionSegment> timeline = {
        ExecutionSegment::idle(0, 3),
        ExecutionSegment::process("P1", 3, 7)
    };

    std::ostringstream out;
    CliController::format_timeline(out, timeline);

    std::string str = out.str();
    EXPECT_NE(str.find("[0 - 3: IDLE] [3 - 7: P1]"), std::string::npos);
}

TEST(CliTest, FormatTimelineEmpty) {
    std::vector<ExecutionSegment> timeline;
    std::ostringstream out;
    CliController::format_timeline(out, timeline);

    std::string str = out.str();
    EXPECT_NE(str.find("[Empty timeline]"), std::string::npos);
}

TEST(CliTest, EofDuringWorkloadInputExitsCleanly) {
    // Choice 1 (FCFS), then 2 processes, then EOF
    std::string in_str = "1\n2\n";
    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    EXPECT_NE(output.str().find("Thank you for using the CPU Scheduling Simulator."), std::string::npos);
}

TEST(CliTest, EofDuringMlfqConfigExitsCleanly) {
    // Choice 8 (MLFQ), then EOF
    std::string in_str = "8\n";
    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    EXPECT_NE(output.str().find("Thank you for using the CPU Scheduling Simulator."), std::string::npos);
}

TEST(CliTest, EofDuringMlqConfigExitsCleanly) {
    // Choice 7 (MLQ), then EOF
    std::string in_str = "7\n";
    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int exit_code = cli.run();

    EXPECT_EQ(exit_code, 0);
    EXPECT_NE(output.str().find("Thank you for using the CPU Scheduling Simulator."), std::string::npos);
}

} // namespace
} // namespace cpu_scheduler
