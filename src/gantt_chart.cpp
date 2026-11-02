#include "cpu_scheduler/gantt_chart.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <vector>

namespace cpu_scheduler {

std::string GanttChart::render(const SchedulingResult& result) {
    return render(result.timeline());
}

std::string GanttChart::render(std::span<const ExecutionSegment> timeline) {
    if (timeline.empty()) {
        return "[Empty Timeline]\n";
    }

    struct ColumnInfo {
        std::string label;
        std::string start_str;
        std::string end_str;
        std::size_t width;
    };

    std::vector<ColumnInfo> columns;
    columns.reserve(timeline.size());

    for (const auto& seg : timeline) {
        std::string label = seg.is_idle() ? "Idle" : seg.process_id.value_or("?");
        std::string start_str = std::to_string(seg.start_time);
        std::string end_str = std::to_string(seg.end_time);

        std::size_t max_content = std::max({label.size(), start_str.size(), end_str.size()});
        std::size_t width = max_content + 2; // minimum 1 space padding on each side

        columns.push_back(ColumnInfo{
            .label = std::move(label),
            .start_str = std::move(start_str),
            .end_str = std::move(end_str),
            .width = width
        });
    }

    // Build Bar Line: |  P1  |  P2  | Idle |  P3  |
    std::ostringstream bar_stream;
    for (const auto& col : columns) {
        bar_stream << "|";
        const std::size_t pad_total = col.width - col.label.size();
        const std::size_t pad_left = pad_total / 2;
        const std::size_t pad_right = pad_total - pad_left;

        bar_stream << std::string(pad_left, ' ') << col.label << std::string(pad_right, ' ');
    }
    bar_stream << "|\n";

    // Build Time Line: 0      5      8      10     14
    std::ostringstream time_stream;
    for (std::size_t i = 0; i < columns.size(); ++i) {
        const auto& col = columns[i];
        time_stream << col.start_str;

        // Space out until the next delimiter position
        // The distance from this '|' to the next '|' is (col.width + 1)
        const std::size_t next_boundary_distance = col.width + 1;
        if (next_boundary_distance > col.start_str.size()) {
            time_stream << std::string(next_boundary_distance - col.start_str.size(), ' ');
        } else {
            time_stream << " ";
        }
    }
    // Print final end timestamp
    time_stream << columns.back().end_str << "\n";

    return bar_stream.str() + time_stream.str();
}

} // namespace cpu_scheduler
