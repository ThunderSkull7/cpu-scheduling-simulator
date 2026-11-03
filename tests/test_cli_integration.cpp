#include "cpu_scheduler/cli.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace cpu_scheduler {
namespace {

// ============================================================================
// Workflow A — Manual FCFS Multi-Process End-to-End
// ============================================================================

TEST(CliIntegrationTest, WorkflowA_ManualFcfsMultiProcess) {
    // Menu: 1 (FCFS)
    // Process count: 3
    // P1: arr 0, burst 3
    // P2: arr 2, burst 2
    // P3: arr 5, burst 4
    // Post-simulation choice: 2 (Exit)
    std::string input_script =
        "1\n"
        "3\n"
        "P1\n0\n3\n"
        "P2\n2\n2\n"
        "P3\n5\n4\n"
        "2\n";

    std::istringstream input(input_script);
    std::ostringstream output;

    CliController cli(input, output);
    int code = cli.run();

    EXPECT_EQ(code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("SIMULATION SUMMARY"), std::string::npos);
    EXPECT_NE(out.find("First Come, First Served"), std::string::npos);
    EXPECT_NE(out.find("P1"), std::string::npos);
    EXPECT_NE(out.find("P2"), std::string::npos);
    EXPECT_NE(out.find("P3"), std::string::npos);
    EXPECT_NE(out.find("Gantt Chart:"), std::string::npos);
    EXPECT_NE(out.find("Execution Timeline:"), std::string::npos);
}

// ============================================================================
// Workflow B — CSV Workload Import with Preemptive Priority Execution
// ============================================================================

TEST(CliIntegrationTest, WorkflowB_CsvWorkloadImportExecution) {
    std::string csv_file = "test_cli_workflow_b.csv";
    {
        std::ofstream f(csv_file);
        f << "process_id,arrival_time,burst_time,priority\n";
        f << "Proc_A,0,4,2\n";
        f << "Proc_B,1,2,1\n";
    }

    // Menu: 6 (Preemptive Priority)
    // Process count: 0 (Import from CSV)
    // Path: test_cli_workflow_b.csv
    // Post-simulation choice: 2 (Exit)
    std::string input_script = "6\n0\n" + csv_file + "\n2\n";

    std::istringstream input(input_script);
    std::ostringstream output;

    CliController cli(input, output);
    int code = cli.run();

    EXPECT_EQ(code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Successfully imported 2 process(es)"), std::string::npos);
    EXPECT_NE(out.find("Preemptive Priority"), std::string::npos);
    EXPECT_NE(out.find("Proc_A"), std::string::npos);
    EXPECT_NE(out.find("Proc_B"), std::string::npos);

    std::error_code ec;
    std::filesystem::remove(csv_file, ec);
}

// ============================================================================
// Workflow C — Invalid CSV Path and Syntax Recovery
// ============================================================================

TEST(CliIntegrationTest, WorkflowC_InvalidCsvRecovery) {
    std::string bad_syntax_csv = "test_cli_bad_syntax.csv";
    std::string good_csv = "test_cli_good.csv";

    {
        std::ofstream f(bad_syntax_csv);
        f << "process_id,arrival_time,burst_time\n";
        f << "P1,not_a_number,5\n"; // Invalid integer
    }
    {
        std::ofstream f(good_csv);
        f << "process_id,arrival_time,burst_time\n";
        f << "P_Good,0,3\n";
    }

    // Sequence:
    // 1 (FCFS)
    // 0 (Import CSV)
    // non_existent_file.csv (reports error)
    // test_cli_bad_syntax.csv (reports syntax error)
    // test_cli_good.csv (succeeds)
    // 2 (Exit)
    std::string input_script =
        "1\n"
        "0\n"
        "non_existent_file.csv\n"
        + bad_syntax_csv + "\n"
        + good_csv + "\n"
        "2\n";

    std::istringstream input(input_script);
    std::ostringstream output;

    CliController cli(input, output);
    int code = cli.run();

    EXPECT_EQ(code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Unable to open file"), std::string::npos);
    EXPECT_NE(out.find("must be a valid integer"), std::string::npos);
    EXPECT_NE(out.find("Successfully imported 1 process(es)"), std::string::npos);
    EXPECT_NE(out.find("P_Good"), std::string::npos);

    std::error_code ec;
    std::filesystem::remove(bad_syntax_csv, ec);
    std::filesystem::remove(good_csv, ec);
}

// ============================================================================
// Workflow D — Round Robin Invalid Quantum Rejection and Execution
// ============================================================================

TEST(CliIntegrationTest, WorkflowD_RoundRobinQuantumReprompt) {
    // Menu: 4 (Round Robin)
    // Quantum prompt: enter 0 (invalid), enter -3 (invalid), enter "abc" (invalid), enter 3 (valid)
    // Processes: 1 process (P1, arr 0, burst 5)
    // Post-simulation: 2 (Exit)
    std::string input_script =
        "4\n"
        "0\n"
        "-3\n"
        "abc\n"
        "3\n"
        "1\n"
        "P1\n0\n5\n"
        "2\n";

    std::istringstream input(input_script);
    std::ostringstream output;

    CliController cli(input, output);
    int code = cli.run();

    EXPECT_EQ(code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Round Robin (RR)"), std::string::npos);
    EXPECT_NE(out.find("P1"), std::string::npos);
}

// ============================================================================
// Workflow E — Interactive MLQ Multi-Queue Setup and Execution
// ============================================================================

TEST(CliIntegrationTest, WorkflowE_InteractiveMlqExecution) {
    // Menu: 7 (MLQ)
    // Number of queues: 2
    // Queue 1: ID Q0, Priority 0, Algorithm 1 (FCFS)
    // Queue 2: ID Q1, Priority 1, Algorithm 1 (FCFS)
    // Policy: 1 (Fixed Priority Preemptive)
    // Process count: 2
    // Process 1: ID P1, arr 0, burst 3, Queue Q0
    // Process 2: ID P2, arr 1, burst 4, Queue Q1
    // Post-simulation: 2 (Exit)
    std::string input_script =
        "7\n"
        "2\n"
        "Q0\n0\n1\n"
        "Q1\n1\n1\n"
        "1\n"
        "2\n"
        "P1\n0\n3\n0\nQ0\n"
        "P2\n1\n4\n0\nQ1\n"
        "2\n";

    std::istringstream input(input_script);
    std::ostringstream output;

    CliController cli(input, output);
    int code = cli.run();

    EXPECT_EQ(code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Multilevel Queue (MLQ)"), std::string::npos);
    EXPECT_NE(out.find("P1"), std::string::npos);
    EXPECT_NE(out.find("P2"), std::string::npos);
}

// ============================================================================
// Workflow F — Interactive MLFQ Configuration and Execution
// ============================================================================

TEST(CliIntegrationTest, WorkflowF_InteractiveMlfqExecution) {
    // Menu: 8 (MLFQ)
    // Levels: 2
    // Level 0 allotment: 2
    // Level 1 allotment: 4
    // Enable boost: y
    // Boost interval: 15
    // Processes: 1 process (P1, arr 0, burst 5)
    // Post-simulation: 2 (Exit)
    std::string input_script =
        "8\n"
        "2\n"
        "2\nn\n"
        "4\nn\n"
        "y\n15\n"
        "1\n"
        "P1\n0\n5\n"
        "2\n";

    std::istringstream input(input_script);
    std::ostringstream output;

    CliController cli(input, output);
    int code = cli.run();

    EXPECT_EQ(code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Multilevel Feedback Queue (MLFQ)"), std::string::npos);
    EXPECT_NE(out.find("P1"), std::string::npos);
}

// ============================================================================
// Workflow G — Post-Simulation CSV Export and Overwrite Decision
// ============================================================================

TEST(CliIntegrationTest, WorkflowG_PostSimExportAndOverwriteCheck) {
    std::string metrics_path = "test_cli_workflow_g_metrics.csv";
    std::string timeline_path = "test_cli_workflow_g_timeline.csv";

    std::error_code ec;
    std::filesystem::remove(metrics_path, ec);
    std::filesystem::remove(timeline_path, ec);

    // Run FCFS with 1 process
    // Select 3 (Export results to CSV)
    // Select 1 (Export process metrics) -> metrics_path
    // Select 2 (Export timeline) -> timeline_path
    // Select 1 again -> metrics_path already exists -> enter 'n' to not overwrite
    // Select 4 (Return)
    // Select 2 (Exit)
    std::string input_script =
        "1\n"
        "1\n"
        "P1\n0\n3\n"
        "3\n"
        "1\n" + metrics_path + "\n"
        "2\n" + timeline_path + "\n"
        "1\n" + metrics_path + "\n"
        "n\n" // Overwrite: n
        "4\n"
        "2\n";

    std::istringstream input(input_script);
    std::ostringstream output;

    CliController cli(input, output);
    int code = cli.run();

    EXPECT_EQ(code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Successfully exported process metrics"), std::string::npos);
    EXPECT_NE(out.find("Successfully exported execution timeline"), std::string::npos);
    EXPECT_NE(out.find("Destination file already exists"), std::string::npos);

    EXPECT_TRUE(std::filesystem::exists(metrics_path));
    EXPECT_TRUE(std::filesystem::exists(timeline_path));

    std::filesystem::remove(metrics_path, ec);
    std::filesystem::remove(timeline_path, ec);
}

// ============================================================================
// Workflow H — EOF Handling at Every Input Stage
// ============================================================================

TEST(CliIntegrationTest, WorkflowH_EofAtEveryInputStageExitsCleanly) {
    // 1. EOF at workload count
    {
        std::istringstream in("1\n"); // Select FCFS, then EOF
        std::ostringstream out;
        CliController cli(in, out);
        EXPECT_EQ(cli.run(), 0);
    }
    // 2. EOF at process ID
    {
        std::istringstream in("1\n1\n"); // FCFS, 1 proc, then EOF at PID
        std::ostringstream out;
        CliController cli(in, out);
        EXPECT_EQ(cli.run(), 0);
    }
    // 3. EOF at arrival time
    {
        std::istringstream in("1\n1\nP1\n"); // FCFS, 1 proc, P1, then EOF at arrival
        std::ostringstream out;
        CliController cli(in, out);
        EXPECT_EQ(cli.run(), 0);
    }
    // 4. EOF at burst time
    {
        std::istringstream in("1\n1\nP1\n0\n"); // FCFS, 1 proc, P1, arr 0, then EOF at burst
        std::ostringstream out;
        CliController cli(in, out);
        EXPECT_EQ(cli.run(), 0);
    }
    // 5. EOF at priority
    {
        std::istringstream in("5\n1\nP1\n0\n4\n"); // NonPreemptivePriority, 1 proc, then EOF at priority
        std::ostringstream out;
        CliController cli(in, out);
        EXPECT_EQ(cli.run(), 0);
    }
    // 6. EOF at Round Robin quantum
    {
        std::istringstream in("4\n"); // Round Robin, then EOF at quantum
        std::ostringstream out;
        CliController cli(in, out);
        EXPECT_EQ(cli.run(), 0);
    }
    // 7. EOF at MLQ queue count
    {
        std::istringstream in("7\n"); // MLQ, then EOF at queue count
        std::ostringstream out;
        CliController cli(in, out);
        EXPECT_EQ(cli.run(), 0);
    }
    // 8. EOF at MLFQ levels count
    {
        std::istringstream in("8\n"); // MLFQ, then EOF at levels count
        std::ostringstream out;
        CliController cli(in, out);
        EXPECT_EQ(cli.run(), 0);
    }
    // 9. EOF at post-simulation menu
    {
        // 1 proc, FCFS, runs, then EOF at post-simulation prompt
        std::istringstream in("1\n1\nP1\n0\n2\n");
        std::ostringstream out;
        CliController cli(in, out);
        EXPECT_EQ(cli.run(), 0);
    }
}

// ============================================================================
// Workflow I — Out-of-Bounds and Malformed Menu Entries Reprompt
// ============================================================================

TEST(CliIntegrationTest, WorkflowI_MenuInputValidationAndReprompts) {
    // Menu: invalid inputs (0, 10, -1, "help"), then 9 (Exit)
    std::string input_script = "0\n10\n-1\nhelp\n9\n";

    std::istringstream input(input_script);
    std::ostringstream output;

    CliController cli(input, output);
    int code = cli.run();

    EXPECT_EQ(code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Error: Invalid input."), std::string::npos);
    EXPECT_NE(out.find("Thank you for using the CPU Scheduling Simulator."), std::string::npos);
}

} // namespace
} // namespace cpu_scheduler
