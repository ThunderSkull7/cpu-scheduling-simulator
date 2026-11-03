#pragma once

#include "cpu_scheduler/gantt_chart.hpp"
#include "cpu_scheduler/mlfq_scheduler.hpp"
#include "cpu_scheduler/mlq_scheduler.hpp"
#include "cpu_scheduler/process.hpp"
#include "cpu_scheduler/process_validator.hpp"
#include "cpu_scheduler/scheduling_engine.hpp"
#include "cpu_scheduler/scheduling_result.hpp"

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cpu_scheduler {

/**
 * @brief Interactive Command-Line Interface controller for the CPU Scheduling Simulator.
 *
 * Implements a decoupled presentation layer that routes all execution through
 * the unified SchedulingEngine. Stream-based design allows full headless automated
 * testing via std::istringstream and std::ostringstream.
 */
class CliController {
public:
    /**
     * @brief Constructs a CLI controller with custom or standard I/O streams.
     *
     * @param in Input stream (e.g. std::cin for production, std::istringstream for tests).
     * @param out Output stream (e.g. std::cout for production, std::ostringstream for tests).
     */
    explicit CliController(std::istream& in, std::ostream& out) noexcept;

    /**
     * @brief Executes the interactive CLI main loop until user exits or input stream ends.
     *
     * @return Process exit code (0 for clean exit).
     */
    int run();

    // Static formatting utilities (public for unit testing and modular reuse)
    static void format_banner(std::ostream& out);
    static void format_main_menu(std::ostream& out);
    static void format_simulation_summary(std::ostream& out,
                                          const SchedulingRequest& request,
                                          const SchedulingResult& result);
    static void format_process_table(std::ostream& out,
                                     const SchedulingResult& result,
                                     bool show_priority,
                                     bool show_queue);
    static void format_aggregate_metrics(std::ostream& out,
                                         const SchedulingMetrics& metrics);
    static void format_gantt_chart(std::ostream& out,
                                   const SchedulingResult& result,
                                   const GanttOptions& options = {});
    static void format_timeline(std::ostream& out,
                                const std::vector<ExecutionSegment>& timeline);
    static void format_error(std::ostream& out,
                             const ScheduleError& error);
    static void format_csv_export_menu(std::ostream& out);

    // Prompting helpers
    [[nodiscard]] std::optional<int> prompt_menu_choice();
    [[nodiscard]] std::optional<std::vector<Process>> prompt_workload(SchedulingAlgorithm algo);
    [[nodiscard]] std::optional<std::vector<Process>> prompt_csv_workload();
    void prompt_csv_export_menu(const SchedulingResult& result);
    [[nodiscard]] std::optional<RoundRobinConfig> prompt_rr_config();
    [[nodiscard]] std::optional<MlqWorkload> prompt_mlq_workload();
    [[nodiscard]] std::optional<MlfqConfig> prompt_mlfq_config();
    [[nodiscard]] std::optional<int> prompt_post_simulation_choice();

    // Low-level safe input parsers
    [[nodiscard]] std::optional<std::string> read_line();
    [[nodiscard]] std::optional<std::int64_t> read_int64(std::string_view prompt,
                                                         std::int64_t min_val,
                                                         std::int64_t max_val);
    [[nodiscard]] std::optional<int> read_int(std::string_view prompt,
                                              int min_val,
                                              int max_val);
    [[nodiscard]] std::optional<std::string> read_non_empty_string(std::string_view prompt);

private:
    [[nodiscard]] bool execute_simulation_cycle();

    std::istream& in_;
    std::ostream& out_;
};

} // namespace cpu_scheduler
