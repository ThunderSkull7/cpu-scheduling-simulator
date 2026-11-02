#pragma once

#include "cpu_scheduler/execution_segment.hpp"
#include "cpu_scheduler/scheduling_result.hpp"

#include <span>
#include <string>

namespace cpu_scheduler {

/**
 * @brief Independent renderer for text-based Gantt charts.
 *
 * Visualizes chronological CPU execution timelines and idle intervals.
 * Contains strictly zero scheduling logic.
 */
class GanttChart {
public:
    GanttChart() = delete;

    /**
     * @brief Renders a clean ASCII Gantt chart from a SchedulingResult.
     * @param result The completed scheduling result.
     * @return Formatted multi-line ASCII string.
     */
    [[nodiscard]] static std::string render(const SchedulingResult& result);

    /**
     * @brief Renders a clean ASCII Gantt chart from an execution timeline.
     * @param timeline Collection of chronologically ordered execution segments.
     * @return Formatted multi-line ASCII string.
     */
    [[nodiscard]] static std::string render(std::span<const ExecutionSegment> timeline);
};

} // namespace cpu_scheduler
