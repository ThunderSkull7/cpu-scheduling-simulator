#include "cpu_scheduler/gantt_chart.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace cpu_scheduler {

namespace {

struct SegmentDisplayInfo {
    std::string label;
    std::string start_str;
    std::string end_str;
    Time duration{0};
    std::size_t width{0};
};

struct RowBorders {
    std::string top_start;
    std::string top_mid;
    std::string top_end;
    std::string top_horiz;

    std::string mid_vert;

    std::string bot_start;
    std::string bot_mid;
    std::string bot_end;
    std::string bot_horiz;
};

RowBorders get_borders(BorderStyle style) {
    if (style == BorderStyle::Unicode) {
        return RowBorders{
            .top_start = "\xE2\x94\x8C", // ┌
            .top_mid = "\xE2\x94\xAC",   // ┬
            .top_end = "\xE2\x94\x90",   // ┐
            .top_horiz = "\xE2\x94\x80", // ─
            .mid_vert = "\xE2\x94\x82",  // │
            .bot_start = "\xE2\x94\x94", // └
            .bot_mid = "\xE2\x94\xB4",   // ┴
            .bot_end = "\xE2\x94\x98",   // ┘
            .bot_horiz = "\xE2\x94\x80"  // ─
        };
    }

    return RowBorders{
        .top_start = "+",
        .top_mid = "+",
        .top_end = "+",
        .top_horiz = "-",
        .mid_vert = "|",
        .bot_start = "+",
        .bot_mid = "+",
        .bot_end = "+",
        .bot_horiz = "-"
    };
}

std::string render_timeline_internal(std::span<const ExecutionSegment> timeline,
                                     const GanttOptions& options,
                                     const std::unordered_map<std::string, std::string>& queue_map) {
    if (timeline.empty()) {
        return "[Empty Timeline]\n";
    }

    Time min_duration = -1;
    Time max_duration = -1;

    for (const auto& seg : timeline) {
        Time dur = seg.duration();
        if (dur > 0) {
            if (min_duration < 0 || dur < min_duration) {
                min_duration = dur;
            }
            if (dur > max_duration) {
                max_duration = dur;
            }
        }
    }

    if (min_duration < 0) {
        min_duration = 1;
        max_duration = 1;
    }

    std::vector<SegmentDisplayInfo> display_segs;
    display_segs.reserve(timeline.size());

    for (const auto& seg : timeline) {
        std::string label;
        if (seg.is_idle()) {
            label = "Idle";
        } else {
            std::string pid = seg.process_id.value_or("?");
            label = pid;
            if (options.show_queue_id) {
                auto it = queue_map.find(pid);
                if (it != queue_map.end() && !it->second.empty()) {
                    label += " (" + it->second + ")";
                }
            }
        }

        std::string start_str = std::to_string(seg.start_time);
        std::string end_str = std::to_string(seg.end_time);
        Time dur = seg.duration();

        std::size_t base_width = std::max(options.min_block_width, label.size() + 2);
        base_width = std::max(base_width, start_str.size() + 1);
        base_width = std::max(base_width, end_str.size() + 1);

        std::size_t width = base_width;
        if (options.proportional_scaling && max_duration > min_duration && dur > min_duration && options.max_block_width > base_width) {
            double ratio = static_cast<double>(dur - min_duration) / static_cast<double>(max_duration - min_duration);
            std::size_t extra = static_cast<std::size_t>(ratio * static_cast<double>(options.max_block_width - base_width));
            width = base_width + extra;
        }

        if (width > options.max_block_width && label.size() + 2 <= options.max_block_width) {
            width = options.max_block_width;
        }

        display_segs.push_back(SegmentDisplayInfo{
            .label = std::move(label),
            .start_str = std::move(start_str),
            .end_str = std::move(end_str),
            .duration = dur,
            .width = width
        });
    }

    // Partition into rows based on options.max_row_width
    std::vector<std::vector<std::size_t>> rows;
    std::vector<std::size_t> current_row;
    std::size_t current_row_width = 1; // initial divider

    for (std::size_t i = 0; i < display_segs.size(); ++i) {
        std::size_t seg_w = display_segs[i].width + 1; // block width + 1 divider

        if (options.max_row_width > 0 && !current_row.empty() && (current_row_width + seg_w > options.max_row_width)) {
            rows.push_back(std::move(current_row));
            current_row.clear();
            current_row_width = 1;
        }

        current_row.push_back(i);
        current_row_width += seg_w;
    }

    if (!current_row.empty()) {
        rows.push_back(std::move(current_row));
    }

    RowBorders borders = get_borders(options.border_style);
    std::ostringstream result_stream;

    for (std::size_t r = 0; r < rows.size(); ++r) {
        const auto& row_indices = rows[r];

        // 1. Top border
        result_stream << borders.top_start;
        for (std::size_t j = 0; j < row_indices.size(); ++j) {
            std::size_t idx = row_indices[j];
            for (std::size_t k = 0; k < display_segs[idx].width; ++k) {
                result_stream << borders.top_horiz;
            }
            if (j + 1 < row_indices.size()) {
                result_stream << borders.top_mid;
            } else {
                result_stream << borders.top_end;
            }
        }
        result_stream << "\n";

        // 2. Middle label line
        result_stream << borders.mid_vert;
        for (std::size_t j = 0; j < row_indices.size(); ++j) {
            std::size_t idx = row_indices[j];
            const auto& seg = display_segs[idx];
            std::size_t pad_total = (seg.width >= seg.label.size()) ? (seg.width - seg.label.size()) : 0;
            std::size_t pad_left = pad_total / 2;
            std::size_t pad_right = pad_total - pad_left;

            result_stream << std::string(pad_left, ' ')
                          << seg.label
                          << std::string(pad_right, ' ')
                          << borders.mid_vert;
        }
        result_stream << "\n";

        // 3. Bottom border
        result_stream << borders.bot_start;
        for (std::size_t j = 0; j < row_indices.size(); ++j) {
            std::size_t idx = row_indices[j];
            for (std::size_t k = 0; k < display_segs[idx].width; ++k) {
                result_stream << borders.bot_horiz;
            }
            if (j + 1 < row_indices.size()) {
                result_stream << borders.bot_mid;
            } else {
                result_stream << borders.bot_end;
            }
        }
        result_stream << "\n";

        // 4. Timestamp line
        // Calculate exact line width needed
        std::size_t total_cols = 1;
        for (std::size_t idx : row_indices) {
            total_cols += display_segs[idx].width + 1;
        }
        total_cols += display_segs[row_indices.back()].end_str.size() + 8;

        std::string time_line(total_cols, ' ');

        // Boundary 0:
        const auto& first_seg = display_segs[row_indices.front()];
        for (std::size_t c = 0; c < first_seg.start_str.size(); ++c) {
            time_line[c] = first_seg.start_str[c];
        }

        std::size_t boundary_pos = 0;
        for (std::size_t j = 0; j < row_indices.size(); ++j) {
            std::size_t idx = row_indices[j];
            boundary_pos += display_segs[idx].width + 1;
            const auto& cur_seg = display_segs[idx];

            for (std::size_t c = 0; c < cur_seg.end_str.size(); ++c) {
                time_line[boundary_pos + c] = cur_seg.end_str[c];
            }
        }

        // Trim trailing whitespace from time_line
        auto last_char = time_line.find_last_not_of(' ');
        if (last_char != std::string::npos) {
            time_line.resize(last_char + 1);
        }
        result_stream << time_line << "\n";

        if (r + 1 < rows.size()) {
            result_stream << "\n";
        }
    }

    return result_stream.str();
}

} // namespace

std::string GanttChart::render(const SchedulingResult& result,
                               const GanttOptions& options) {
    std::unordered_map<std::string, std::string> queue_map;
    for (const auto& proc : result.processes()) {
        if (!proc.queue_id.empty()) {
            queue_map[proc.process_id] = proc.queue_id;
        }
    }

    GanttOptions opts = options;
    if (!queue_map.empty() && !options.show_queue_id) {
        opts.show_queue_id = true;
    }

    return render_timeline_internal(result.timeline(), opts, queue_map);
}

std::string GanttChart::render(std::span<const ExecutionSegment> timeline,
                               const GanttOptions& options) {
    std::unordered_map<std::string, std::string> empty_queue_map;
    return render_timeline_internal(timeline, options, empty_queue_map);
}

} // namespace cpu_scheduler
