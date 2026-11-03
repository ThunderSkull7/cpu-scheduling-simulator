#include "cpu_scheduler/workload_importer.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

namespace cpu_scheduler {
namespace {

TEST(WorkloadImporterTest, ValidMinimalWorkload) {
    const std::string csv =
        "process_id,arrival_time,burst_time\n"
        "P1,0,5\n"
        "P2,2,3\n";

    auto res = WorkloadImporter::import_from_string(csv);
    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.processes.size(), 2u);

    EXPECT_EQ(res.processes[0].id, "P1");
    EXPECT_EQ(res.processes[0].arrival_time, 0);
    EXPECT_EQ(res.processes[0].burst_time, 5);
    EXPECT_EQ(res.processes[0].priority, 0);

    EXPECT_EQ(res.processes[1].id, "P2");
    EXPECT_EQ(res.processes[1].arrival_time, 2);
    EXPECT_EQ(res.processes[1].burst_time, 3);
    EXPECT_EQ(res.processes[1].priority, 0);
}

TEST(WorkloadImporterTest, ValidWorkloadWithPriorityAndQueueId) {
    const std::string csv =
        "process_id,arrival_time,burst_time,priority,queue_id\n"
        "P1,0,4,2,Q_Interactive\n"
        "P2,1,6,-1,Q_Batch\n";

    auto res = WorkloadImporter::import_from_string(csv);
    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.processes.size(), 2u);
    EXPECT_TRUE(res.has_priority);
    EXPECT_TRUE(res.has_queue_id);

    EXPECT_EQ(res.processes[0].id, "P1");
    EXPECT_EQ(res.processes[0].priority, 2);
    ASSERT_EQ(res.queue_bindings.size(), 2u);
    EXPECT_EQ(res.queue_bindings[0], "Q_Interactive");

    EXPECT_EQ(res.processes[1].id, "P2");
    EXPECT_EQ(res.processes[1].priority, -1);
    EXPECT_EQ(res.queue_bindings[1], "Q_Batch");
}

TEST(WorkloadImporterTest, CaseInsensitiveHeadersAndWhitespaceTrimming) {
    const std::string csv =
        "  PROCESS_ID , Arrival_Time ,  BURST_TIME  \n"
        " P1 , 0 , 4 \n";

    auto res = WorkloadImporter::import_from_string(csv);
    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.processes.size(), 1u);
    EXPECT_EQ(res.processes[0].id, "P1");
    EXPECT_EQ(res.processes[0].arrival_time, 0);
    EXPECT_EQ(res.processes[0].burst_time, 4);
}

TEST(WorkloadImporterTest, MissingRequiredColumnRejected) {
    const std::string csv =
        "process_id,burst_time\n"
        "P1,5\n";

    auto res = WorkloadImporter::import_from_string(csv);
    EXPECT_FALSE(res.success);
    EXPECT_FALSE(res.errors.empty());
    EXPECT_NE(res.errors[0].message.find("Missing required column 'arrival_time'"), std::string::npos);
}

TEST(WorkloadImporterTest, DuplicateHeaderRejected) {
    const std::string csv =
        "process_id,arrival_time,burst_time,pid\n"
        "P1,0,5,P1\n";

    auto res = WorkloadImporter::import_from_string(csv);
    EXPECT_FALSE(res.success);
    EXPECT_FALSE(res.errors.empty());
    bool found_dup_or_ambig = false;
    for (const auto& err : res.errors) {
        if (err.message.find("Ambiguous header") != std::string::npos ||
            err.message.find("Duplicate column") != std::string::npos) {
            found_dup_or_ambig = true;
        }
    }
    EXPECT_TRUE(found_dup_or_ambig);
}

TEST(WorkloadImporterTest, UnsupportedColumnRejected) {
    const std::string csv =
        "process_id,arrival_time,burst_time,unsupported_column\n"
        "P1,0,5,extra\n";

    auto res = WorkloadImporter::import_from_string(csv);
    EXPECT_FALSE(res.success);
    EXPECT_FALSE(res.errors.empty());
    EXPECT_NE(res.errors[0].message.find("Unsupported column header"), std::string::npos);
}

TEST(WorkloadImporterTest, DuplicateProcessIdCaseInsensitiveRejected) {
    const std::string csv =
        "process_id,arrival_time,burst_time\n"
        "P1,0,5\n"
        "p1,2,3\n";

    auto res = WorkloadImporter::import_from_string(csv);
    EXPECT_FALSE(res.success);
    ASSERT_FALSE(res.errors.empty());
    EXPECT_NE(res.errors[0].message.find("Duplicate process ID 'p1'"), std::string::npos);
    EXPECT_EQ(res.errors[0].row, 3u);
}

TEST(WorkloadImporterTest, EmptyProcessIdRejected) {
    const std::string csv =
        "process_id,arrival_time,burst_time\n"
        "   ,0,5\n";

    auto res = WorkloadImporter::import_from_string(csv);
    EXPECT_FALSE(res.success);
    ASSERT_FALSE(res.errors.empty());
    EXPECT_NE(res.errors[0].message.find("Process ID cannot be empty"), std::string::npos);
    EXPECT_EQ(res.errors[0].row, 2u);
}

TEST(WorkloadImporterTest, NegativeArrivalTimeRejected) {
    const std::string csv =
        "process_id,arrival_time,burst_time\n"
        "P1,-3,5\n";

    auto res = WorkloadImporter::import_from_string(csv);
    EXPECT_FALSE(res.success);
    ASSERT_FALSE(res.errors.empty());
    EXPECT_NE(res.errors[0].message.find("cannot be negative"), std::string::npos);
    EXPECT_EQ(res.errors[0].row, 2u);
}

TEST(WorkloadImporterTest, NonIntegerArrivalTimeRejected) {
    const std::string csv =
        "process_id,arrival_time,burst_time\n"
        "P1,3.5,5\n";

    auto res = WorkloadImporter::import_from_string(csv);
    EXPECT_FALSE(res.success);
    ASSERT_FALSE(res.errors.empty());
    EXPECT_NE(res.errors[0].message.find("must be a valid integer"), std::string::npos);
}

TEST(WorkloadImporterTest, ZeroBurstTimeRejected) {
    const std::string csv =
        "process_id,arrival_time,burst_time\n"
        "P1,0,0\n";

    auto res = WorkloadImporter::import_from_string(csv);
    EXPECT_FALSE(res.success);
    ASSERT_FALSE(res.errors.empty());
    EXPECT_NE(res.errors[0].message.find("must be strictly positive"), std::string::npos);
    EXPECT_EQ(res.errors[0].row, 2u);
}

TEST(WorkloadImporterTest, NegativeBurstTimeRejected) {
    const std::string csv =
        "process_id,arrival_time,burst_time\n"
        "P1,0,-5\n";

    auto res = WorkloadImporter::import_from_string(csv);
    EXPECT_FALSE(res.success);
    ASSERT_FALSE(res.errors.empty());
    EXPECT_NE(res.errors[0].message.find("must be strictly positive"), std::string::npos);
}

TEST(WorkloadImporterTest, InvalidPriorityRejected) {
    const std::string csv =
        "process_id,arrival_time,burst_time,priority\n"
        "P1,0,5,high\n";

    auto res = WorkloadImporter::import_from_string(csv);
    EXPECT_FALSE(res.success);
    ASSERT_FALSE(res.errors.empty());
    EXPECT_NE(res.errors[0].message.find("Priority must be a valid integer"), std::string::npos);
}

TEST(WorkloadImporterTest, FieldCountMismatchRejected) {
    const std::string csv =
        "process_id,arrival_time,burst_time\n"
        "P1,0,5,extra_value\n";

    auto res = WorkloadImporter::import_from_string(csv);
    EXPECT_FALSE(res.success);
    ASSERT_FALSE(res.errors.empty());
    EXPECT_NE(res.errors[0].message.find("Field count mismatch"), std::string::npos);
}

TEST(WorkloadImporterTest, ExceedingMaxRecordsRejected) {
    std::string csv = "process_id,arrival_time,burst_time\n";
    for (int i = 1; i <= 105; ++i) {
        csv += "P" + std::to_string(i) + ",0,5\n";
    }

    auto res = WorkloadImporter::import_from_string(csv, 100);
    EXPECT_FALSE(res.success);
    ASSERT_FALSE(res.errors.empty());
    EXPECT_NE(res.errors[0].message.find("exceeds maximum allowed limit of 100"), std::string::npos);
}

TEST(WorkloadImporterTest, MultipleErrorsCollectedSimultaneously) {
    const std::string csv =
        "process_id,arrival_time,burst_time\n"
        "P1,-1,0\n"
        "P1,2,5\n";

    auto res = WorkloadImporter::import_from_string(csv);
    EXPECT_FALSE(res.success);
    EXPECT_GE(res.errors.size(), 2u);
    EXPECT_TRUE(res.processes.empty()); // All-or-nothing!
}

} // namespace
} // namespace cpu_scheduler
