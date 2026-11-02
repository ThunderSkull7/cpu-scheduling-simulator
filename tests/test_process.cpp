#include "cpu_scheduler/process.hpp"

#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace cpu_scheduler::testing {

TEST(ProcessTest, DefaultConstruction) {
    const Process proc;
    EXPECT_TRUE(proc.id.empty());
    EXPECT_EQ(proc.arrival_time, 0);
    EXPECT_EQ(proc.burst_time, 0);
    EXPECT_EQ(proc.priority, 0);
}

TEST(ProcessTest, CorrectProcessConstruction) {
    const Process proc("P1", 5, 10, 2);
    EXPECT_EQ(proc.id, "P1");
    EXPECT_EQ(proc.arrival_time, 5);
    EXPECT_EQ(proc.burst_time, 10);
    EXPECT_EQ(proc.priority, 2);
}

TEST(ProcessTest, DefaultPriority) {
    const Process proc("P2", 3, 7);
    EXPECT_EQ(proc.id, "P2");
    EXPECT_EQ(proc.arrival_time, 3);
    EXPECT_EQ(proc.burst_time, 7);
    EXPECT_EQ(proc.priority, 0);
}

TEST(ProcessTest, PreservationOfAllOriginalInputFields) {
    // Test large 64-bit integer values to ensure no truncation or mutation
    constexpr Time large_arrival = 1'000'000'000'000LL;
    constexpr Time large_burst = 5'000'000'000'000LL;
    constexpr int priority_val = -42;
    const std::string original_id = "Process_Alpha_99";

    const Process proc(original_id, large_arrival, large_burst, priority_val);

    EXPECT_EQ(proc.id, original_id);
    EXPECT_EQ(proc.arrival_time, large_arrival);
    EXPECT_EQ(proc.burst_time, large_burst);
    EXPECT_EQ(proc.priority, priority_val);
}

TEST(ProcessTest, MultipleProcessesWithDistinctIDs) {
    const Process p1("P1", 0, 5, 1);
    const Process p2("P2", 2, 8, 3);
    const Process p3("P3", 4, 2, 2);

    EXPECT_NE(p1.id, p2.id);
    EXPECT_NE(p2.id, p3.id);
    EXPECT_NE(p1.id, p3.id);

    EXPECT_EQ(p1.burst_time, 5);
    EXPECT_EQ(p2.burst_time, 8);
    EXPECT_EQ(p3.burst_time, 2);

    EXPECT_NE(p1, p2);
    EXPECT_NE(p2, p3);
}

TEST(ProcessTest, EqualityOperator) {
    const Process p1("P1", 2, 5, 1);
    const Process p2("P1", 2, 5, 1);
    const Process p3("P1", 2, 6, 1);

    EXPECT_EQ(p1, p2);
    EXPECT_NE(p1, p3);
}

TEST(ProcessTest, StreamOutputOperator) {
    const Process proc("Task1", 10, 20, 3);
    std::ostringstream oss;
    oss << proc;

    const std::string output = oss.str();
    EXPECT_NE(output.find("Task1"), std::string::npos);
    EXPECT_NE(output.find("10"), std::string::npos);
    EXPECT_NE(output.find("20"), std::string::npos);
    EXPECT_NE(output.find("3"), std::string::npos);
}

} // namespace cpu_scheduler::testing
