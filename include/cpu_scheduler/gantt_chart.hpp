#pragma once

#include "cpu_scheduler/execution_segment.hpp"
#include "cpu_scheduler/scheduling_result.hpp"

#include <cstddef>
#include <span>
#include <string>

namespace cpu_scheduler {

/**
 * @brief Character set style used for rendering Gantt chart borders.
 */
enum class BorderStyle {
    Ascii,  ///< Standard 7-bit ASCII characters (+, -, |) for universal terminal portability
    Unicode ///< UTF-8 box-drawing characters (┌, ─, ┬, ┐, etc.)
};

/**
 * @brief Configuration parameters controlling Gantt chart layout and rendering.
 */
struct GanttOptions {
    std::size_t min_block_width{7};      ///< Minimum character width of an execution block
    std::size_t max_block_width{24};     ///< Maximum character width of an execution block
    std::size_t max_row_width{80};       ///< Terminal column limit before wrapping to next row (0 = unlimited)
    BorderStyle border_style{BorderStyle::Ascii}; ///< Border rendering style (default: Ascii)
    bool show_queue_id{false};           ///< Display assigned queue identifier in block label if present
    bool proportional_scaling{true};     ///< Scale block width proportionally with segment duration

    constexpr GanttOptions() = default;
};

/**
 * @brief Enhanced renderer for text-based terminal Gantt charts.
 *
 * Visualizes chronological CPU execution timelines, process preemption, and idle intervals
 * with proportional scaling, timestamp alignment, and safe row wrapping.
 * Contains strictly zero scheduling or simulation logic.
 */
class GanttChart {
public:
    GanttChart() = delete;

    /**
     * @brief Renders a clean text-based Gantt chart from a SchedulingResult.
     *
     * Automatically extracts execution segments and resolves process metadata (such as queue IDs).
     *
     * @param result The completed scheduling result.
     * @param options Layout, scaling, and border configuration options.
     * @return Formatted multi-line text representation of the Gantt chart.
     */
    [[nodiscard]] static std::string render(const SchedulingResult& result,
                                            const GanttOptions& options = {});

    /**
     * @brief Renders a clean text-based Gantt chart from an execution timeline.
     *
     * @param timeline Collection of chronologically ordered execution segments.
     * @param options Layout, scaling, and border configuration options.
     * @return Formatted multi-line text representation of the Gantt chart.
     */
    [[nodiscard]] static std::string render(std::span<const ExecutionSegment> timeline,
                                            const GanttOptions& options = {});
};

} // namespace cpu_scheduler
