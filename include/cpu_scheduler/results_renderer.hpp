#pragma once

#include "cpu_scheduler/gantt_chart.hpp"
#include "cpu_scheduler/scheduling_engine.hpp"
#include "cpu_scheduler/scheduling_result.hpp"

#include <iosfwd>
#include <span>
#include <string>

namespace cpu_scheduler {

/**
 * @brief Options controlling which sections and metadata to include in the rendered results report.
 */
struct RenderOptions {
    bool show_summary{true};             ///< Include the Simulation Summary block
    bool show_process_table{true};       ///< Include the Process Execution Details table
    bool show_aggregate_metrics{true};   ///< Include the Aggregate Performance Metrics block
    bool show_gantt_chart{true};         ///< Include the rendered visual Gantt chart
    bool show_timeline{true};            ///< Include the compact chronological execution timeline
    bool show_priority{false};           ///< Display Priority column in process table
    bool show_queue{false};              ///< Display Queue ID column in process table
    GanttOptions gantt_options{};        ///< Layout options for the embedded Gantt chart

    constexpr RenderOptions() = default;
};

/**
 * @brief Dedicated presentation engine for formatting scheduling simulation analytics and reports.
 *
 * Fully decoupled from the scheduling engines and CLI controller. Produces structured,
 * human-readable text representations with stream and string overloads for testing.
 */
class ResultsRenderer {
public:
    ResultsRenderer() = delete;

    /**
     * @brief Renders the Simulation Summary section.
     */
    [[nodiscard]] static std::string render_summary(const SchedulingRequest& request,
                                                    const SchedulingResult& result);

    /**
     * @brief Renders the tabular Process Execution Details.
     */
    [[nodiscard]] static std::string render_process_table(const SchedulingResult& result,
                                                          bool show_priority = false,
                                                          bool show_queue = false);

    /**
     * @brief Renders the Aggregate Metrics section.
     */
    [[nodiscard]] static std::string render_aggregate_metrics(const SchedulingMetrics& metrics);

    /**
     * @brief Renders the compact chronological execution timeline trace.
     */
    [[nodiscard]] static std::string render_timeline(std::span<const ExecutionSegment> timeline);

    /**
     * @brief Renders the visual Gantt chart.
     */
    [[nodiscard]] static std::string render_gantt_chart(const SchedulingResult& result,
                                                        const GanttOptions& options = {});

    /**
     * @brief Renders the visual Gantt chart from an execution timeline.
     */
    [[nodiscard]] static std::string render_gantt_chart(std::span<const ExecutionSegment> timeline,
                                                        const GanttOptions& options = {});

    /**
     * @brief Generates a complete human-readable report combining requested sections.
     *
     * @param request The scheduling request executed.
     * @param result The outcome produced by the scheduling engine.
     * @param options Configuration specifying which sections to render.
     * @return Formatted multi-line text report.
     */
    [[nodiscard]] static std::string render_full_report(const SchedulingRequest& request,
                                                        const SchedulingResult& result,
                                                        const RenderOptions& options = {});

    /**
     * @brief Streams the complete results report to an output stream.
     */
    static void render_full_report(std::ostream& out,
                                   const SchedulingRequest& request,
                                   const SchedulingResult& result,
                                   const RenderOptions& options = {});
};

} // namespace cpu_scheduler
