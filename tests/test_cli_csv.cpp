#include "cpu_scheduler/cli.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace cpu_scheduler {
namespace {

TEST(CliCsvTest, ImportWorkloadFromCsvInCli) {
    // Create temporary CSV file
    std::string csv_path = "test_cli_import.csv";
    {
        std::ofstream f(csv_path);
        f << "process_id,arrival_time,burst_time\n";
        f << "P1,0,3\n";
        f << "P2,1,4\n";
    }

    // Input sequence:
    // 1 (FCFS)
    // 0 (Import from CSV)
    // test_cli_import.csv (file path)
    // 2 (Post-sim: Exit)
    std::string in_str = "1\n0\n" + csv_path + "\n2\n";
    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int code = cli.run();

    EXPECT_EQ(code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Successfully imported 2 process(es)"), std::string::npos);
    EXPECT_NE(out.find("P1"), std::string::npos);
    EXPECT_NE(out.find("P2"), std::string::npos);

    std::error_code ec;
    std::filesystem::remove(csv_path, ec);
}

TEST(CliCsvTest, ExportProcessMetricsAndTimelineViaCli) {
    std::string metrics_file = "test_cli_metrics_out.csv";
    std::string timeline_file = "test_cli_timeline_out.csv";

    std::error_code ec;
    std::filesystem::remove(metrics_file, ec);
    std::filesystem::remove(timeline_file, ec);

    // Input sequence:
    // 1 (FCFS)
    // 1 (Enter manually: 1 process)
    // P1, 0, 3
    // 3 (Post-sim: Export results to CSV)
    // 1 (Export process metrics)
    // test_cli_metrics_out.csv
    // 2 (Export timeline)
    // test_cli_timeline_out.csv
    // 4 (Return to post-sim menu)
    // 2 (Exit)
    std::string in_str =
        "1\n"
        "1\n"
        "P1\n0\n3\n"
        "3\n"
        "1\n" + metrics_file + "\n"
        "2\n" + timeline_file + "\n"
        "4\n"
        "2\n";

    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int code = cli.run();

    EXPECT_EQ(code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("Successfully exported process metrics"), std::string::npos);
    EXPECT_NE(out.find("Successfully exported execution timeline"), std::string::npos);

    EXPECT_TRUE(std::filesystem::exists(metrics_file));
    EXPECT_TRUE(std::filesystem::exists(timeline_file));

    std::filesystem::remove(metrics_file, ec);
    std::filesystem::remove(timeline_file, ec);
}

TEST(CliCsvTest, ImportFailureAndRecoveryInCli) {
    std::string bad_csv = "test_bad_csv.csv";
    std::string good_csv = "test_good_csv.csv";

    {
        std::ofstream f(bad_csv);
        f << "process_id,arrival_time,burst_time\n";
        f << "P1,-5,0\n"; // Negative arrival, zero burst
    }
    {
        std::ofstream f(good_csv);
        f << "process_id,arrival_time,burst_time\n";
        f << "P1,0,4\n";
    }

    // Input sequence:
    // 1 (FCFS)
    // 0 (Import from CSV)
    // test_bad_csv.csv -> fails!
    // test_good_csv.csv -> succeeds!
    // 2 (Exit)
    std::string in_str = "1\n0\n" + bad_csv + "\n" + good_csv + "\n2\n";
    std::istringstream input(in_str);
    std::ostringstream output;

    CliController cli(input, output);
    int code = cli.run();

    EXPECT_EQ(code, 0);
    std::string out = output.str();
    EXPECT_NE(out.find("CSV import failed"), std::string::npos);
    EXPECT_NE(out.find("Successfully imported 1 process(es)"), std::string::npos);

    std::error_code ec;
    std::filesystem::remove(bad_csv, ec);
    std::filesystem::remove(good_csv, ec);
}

} // namespace
} // namespace cpu_scheduler
