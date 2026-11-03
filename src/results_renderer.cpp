#include "cpu_scheduler/results_renderer.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cpu_scheduler {

std::string ResultsRenderer::render_summary(const SchedulingRequest& request,
                                            const SchedulingResult& result) {
    std::ostringstream out;
    out << "\n============================================================\n";
    out << "                    SIMULATION SUMMARY\n";
    out << "============================================================\n";
    out << std::left << std::setw(26) << "Algorithm:" << algorithm_name(request.algorithm) << "\n";
    out << std::left << std::setw(26) << "Process Count:" << result.processes().size() << "\n";
    out << std::left << std::setw(26) << "Total Simulation Time:" << result.total_execution_time() << "\n";
    out << std::left << std::setw(26) << "CPU Busy Time:" << result.cpu_busy_time() << "\n";
    out << std::left << std::setw(26) << "CPU Idle Time:" << result.cpu_idle_time() << "\n";
    out << std::left << std::setw(26) << "CPU Utilization:"
        << std::fixed << std::setprecision(2) << result.cpu_utilization() << "%\n";
    out << "============================================================\n";
    return out.str();
}

std::string ResultsRenderer::render_process_table(const SchedulingResult& result,
                                                  bool show_priority,
                                                  bool show_queue) {
    std::ostringstream out;
    out << "\nProcess Execution Details:\n";

    // Table Header
    out << std::left << std::setw(8) << "PID"
        << std::right << std::setw(9) << "Arrival"
        << std::right << std::setw(8) << "Burst";

    if (show_priority) {
        out << std::right << std::setw(11) << "Priority";
    }
    if (show_queue) {
        out << std::setw(10) << "Queue";
    }

    out << std::right << std::setw(12) << "Completion"
        << std::right << std::setw(12) << "Turnaround"
        << std::right << std::setw(10) << "Waiting"
        << std::right << std::setw(10) << "Response"
        << "\n";

    std::size_t line_len = 57 + (show_priority ? 11 : 0) + (show_queue ? 10 : 0);
    out << std::string(line_len, '-') << "\n";

    for (const auto& proc : result.processes()) {
        out << std::left << std::setw(8) << proc.process_id
            << std::right << std::setw(9) << proc.arrival_time
            << std::right << std::setw(8) << proc.burst_time;

        if (show_priority) {
            out << std::right << std::setw(11) << proc.priority;
        }
        if (show_queue) {
            out << std::setw(10) << (proc.queue_id.empty() ? "-" : proc.queue_id);
        }

        out << std::right << std::setw(12) << proc.completion_time
            << std::right << std::setw(12) << proc.turnaround_time
            << std::right << std::setw(10) << proc.waiting_time
            << std::right << std::setw(10) << proc.response_time
            << "\n";
    }
    out << std::string(line_len, '-') << "\n";

    return out.str();
}

std::string ResultsRenderer::render_aggregate_metrics(const SchedulingMetrics& metrics) {
    std::ostringstream out;
    out << "\n============================================================\n";
    out << "                     AGGREGATE METRICS\n";
    out << "============================================================\n";
    out << std::left << std::setw(28) << "Average Turnaround Time:"
        << std::fixed << std::setprecision(2) << metrics.average_turnaround_time << "\n";
    out << std::left << std::setw(28) << "Average Waiting Time:"
        << std::fixed << std::setprecision(2) << metrics.average_waiting_time << "\n";
    out << std::left << std::setw(28) << "Average Response Time:"
        << std::fixed << std::setprecision(2) << metrics.average_response_time << "\n";
    out << std::left << std::setw(28) << "CPU Utilization:"
        << std::fixed << std::setprecision(2) << metrics.cpu_utilization << "%\n";
    out << "============================================================\n";
    return out.str();
}

std::string ResultsRenderer::render_timeline(std::span<const ExecutionSegment> timeline) {
    std::ostringstream out;
    out << "\nExecution Timeline:\n";
    if (timeline.empty()) {
        out << "  [Empty timeline]\n";
        return out.str();
    }

    out << "  ";
    for (std::size_t i = 0; i < timeline.size(); ++i) {
        const auto& seg = timeline[i];
        out << "[" << seg.start_time << " - " << seg.end_time << ": ";
        if (seg.is_idle()) {
            out << "IDLE";
        } else {
            out << seg.process_id.value_or("?");
        }
        out << "]";
        if (i + 1 < timeline.size()) {
            out << " ";
        }
    }
    out << "\n";
    return out.str();
}

std::string ResultsRenderer::render_gantt_chart(const SchedulingResult& result,
                                                const GanttOptions& options) {
    return GanttChart::render(result, options);
}

std::string ResultsRenderer::render_gantt_chart(std::span<const ExecutionSegment> timeline,
                                                const GanttOptions& options) {
    return GanttChart::render(timeline, options);
}

std::string ResultsRenderer::render_full_report(const SchedulingRequest& request,
                                                const SchedulingResult& result,
                                                const RenderOptions& options) {
    std::ostringstream out;
    render_full_report(out, request, result, options);
    return out.str();
}

void ResultsRenderer::render_full_report(std::ostream& out,
                                         const SchedulingRequest& request,
                                         const SchedulingResult& result,
                                         const RenderOptions& options) {
    if (options.show_summary) {
        out << render_summary(request, result);
    }

    if (options.show_process_table) {
        out << render_process_table(result, options.show_priority, options.show_queue);
    }

    if (options.show_aggregate_metrics) {
        out << render_aggregate_metrics(result.metrics());
    }

    if (options.show_gantt_chart) {
        out << "\nGantt Chart:\n";
        out << render_gantt_chart(result, options.gantt_options);
    }

    if (options.show_timeline) {
        out << render_timeline(result.timeline());
    }
}

} // namespace cpu_scheduler
