#include "cpu_scheduler/cli.hpp"

#include "cpu_scheduler/process_validator.hpp"

#include <algorithm>
#include <cctype>
#include <climits>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace cpu_scheduler {

namespace {

std::string trim_string(std::string_view sv) {
    auto start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return "";
    }
    auto end = sv.find_last_not_of(" \t\r\n");
    return std::string(sv.substr(start, end - start + 1));
}

bool parse_int64_exact(std::string_view sv, std::int64_t min_val, std::int64_t max_val, std::int64_t& out_val) {
    std::string trimmed = trim_string(sv);
    if (trimmed.empty()) {
        return false;
    }

    try {
        std::size_t idx = 0;
        long long val = std::stoll(trimmed, &idx);
        if (idx != trimmed.size()) {
            return false;
        }
        if (val < min_val || val > max_val) {
            return false;
        }
        out_val = static_cast<std::int64_t>(val);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

CliController::CliController(std::istream& in, std::ostream& out) noexcept
    : in_(in), out_(out) {}

int CliController::run() {
    format_banner(out_);

    while (true) {
        bool continue_loop = execute_simulation_cycle();
        if (!continue_loop) {
            break;
        }
    }

    out_ << "\nThank you for using the CPU Scheduling Simulator.\n";
    return 0;
}

void CliController::format_banner(std::ostream& out) {
    out << "============================================================\n";
    out << "              CPU SCHEDULING SIMULATOR - CLI\n";
    out << "============================================================\n";
}

void CliController::format_main_menu(std::ostream& out) {
    out << "\nAvailable Scheduling Algorithms:\n";
    out << "  1. First Come, First Served (FCFS)\n";
    out << "  2. Shortest Job First (SJF, Non-Preemptive)\n";
    out << "  3. Shortest Remaining Time First (SRTF, Preemptive)\n";
    out << "  4. Round Robin (RR)\n";
    out << "  5. Priority (Non-Preemptive)\n";
    out << "  6. Priority (Preemptive)\n";
    out << "  7. Multilevel Queue (MLQ)\n";
    out << "  8. Multilevel Feedback Queue (MLFQ)\n";
    out << "  9. Exit\n";
    out << "============================================================\n";
}

void CliController::format_simulation_summary(std::ostream& out,
                                              const SchedulingRequest& request,
                                              const SchedulingResult& result) {
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
}

void CliController::format_process_table(std::ostream& out,
                                        const SchedulingResult& result,
                                        bool show_priority,
                                        bool show_queue) {
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

    // Separator line
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
}

void CliController::format_aggregate_metrics(std::ostream& out,
                                            const SchedulingMetrics& metrics) {
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
}

void CliController::format_timeline(std::ostream& out,
                                    const std::vector<ExecutionSegment>& timeline) {
    out << "\nExecution Timeline:\n";
    if (timeline.empty()) {
        out << "  [Empty timeline]\n";
        return;
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
}

void CliController::format_error(std::ostream& out,
                                 const ScheduleError& error) {
    out << "\n============================================================\n";
    out << "                     SIMULATION FAILED\n";
    out << "============================================================\n";
    out << "Error: " << error.message << "\n";
    if (error.validation_result.has_value() && !error.validation_result->is_valid()) {
        out << "\nDetailed Validation Errors:\n";
        for (const auto& err : error.validation_result->errors()) {
            out << "  - " << err.message << "\n";
        }
    }
    out << "============================================================\n";
}

std::optional<std::string> CliController::read_line() {
    std::string line;
    if (!std::getline(in_, line)) {
        return std::nullopt;
    }
    return trim_string(line);
}

std::optional<std::int64_t> CliController::read_int64(std::string_view prompt,
                                                      std::int64_t min_val,
                                                      std::int64_t max_val) {
    while (true) {
        out_ << prompt;
        auto line = read_line();
        if (!line.has_value()) {
            return std::nullopt;
        }

        std::int64_t val = 0;
        if (parse_int64_exact(*line, min_val, max_val, val)) {
            return val;
        }

        out_ << "Error: Invalid input. Value must be an integer between "
             << min_val << " and " << max_val << ". Please try again.\n";
    }
}

std::optional<int> CliController::read_int(std::string_view prompt,
                                           int min_val,
                                           int max_val) {
    auto res = read_int64(prompt, min_val, max_val);
    if (!res.has_value()) {
        return std::nullopt;
    }
    return static_cast<int>(*res);
}

std::optional<std::string> CliController::read_non_empty_string(std::string_view prompt) {
    while (true) {
        out_ << prompt;
        auto line = read_line();
        if (!line.has_value()) {
            return std::nullopt;
        }
        if (!line->empty()) {
            return *line;
        }
        out_ << "Error: Input cannot be empty. Please try again.\n";
    }
}

std::optional<int> CliController::prompt_menu_choice() {
    format_main_menu(out_);
    return read_int("Enter selection (1-9): ", 1, 9);
}

std::optional<std::vector<Process>> CliController::prompt_workload(SchedulingAlgorithm algo) {
    out_ << "\n--- Workload Configuration ---\n";
    auto num_procs = read_int("Enter number of processes (1-100): ", 1, 100);
    if (!num_procs.has_value()) {
        return std::nullopt;
    }

    std::vector<Process> processes;
    processes.reserve(static_cast<std::size_t>(*num_procs));
    std::unordered_set<std::string> normalized_ids;

    bool collect_priority = (algo == SchedulingAlgorithm::NonPreemptivePriority ||
                             algo == SchedulingAlgorithm::PreemptivePriority);

    for (int i = 1; i <= *num_procs; ++i) {
        out_ << "\nProcess " << i << " of " << *num_procs << ":\n";

        // Process ID loop
        std::string pid;
        while (true) {
            auto id_input = read_non_empty_string("  Process ID (e.g. P1): ");
            if (!id_input.has_value()) {
                return std::nullopt;
            }
            std::string norm_id = ProcessValidator::normalize_id(*id_input);
            if (normalized_ids.contains(norm_id)) {
                out_ << "Error: Process ID '" << *id_input
                     << "' already exists. Process IDs must be unique. Please try again.\n";
                continue;
            }
            normalized_ids.insert(norm_id);
            pid = *id_input;
            break;
        }

        // Arrival time loop
        auto arrival = read_int64("  Arrival time (>= 0): ", 0, std::numeric_limits<std::int64_t>::max());
        if (!arrival.has_value()) {
            return std::nullopt;
        }

        // Burst time loop
        auto burst = read_int64("  Burst time (> 0): ", 1, std::numeric_limits<std::int64_t>::max());
        if (!burst.has_value()) {
            return std::nullopt;
        }

        // Priority
        int prio = 0;
        if (collect_priority) {
            auto p = read_int("  Priority (integer, smaller = higher priority): ",
                              std::numeric_limits<int>::min(),
                              std::numeric_limits<int>::max());
            if (!p.has_value()) {
                return std::nullopt;
            }
            prio = *p;
        }

        processes.emplace_back(pid, *arrival, *burst, prio);
    }

    return processes;
}

std::optional<RoundRobinConfig> CliController::prompt_rr_config() {
    out_ << "\n--- Round Robin Configuration ---\n";
    auto quantum = read_int64("Enter time quantum (> 0): ", 1, std::numeric_limits<std::int64_t>::max());
    if (!quantum.has_value()) {
        return std::nullopt;
    }
    return RoundRobinConfig{*quantum};
}

std::optional<MlqWorkload> CliController::prompt_mlq_workload() {
    out_ << "\n--- Multilevel Queue (MLQ) Queue Setup ---\n";
    auto num_queues = read_int("Enter number of queues (1-10): ", 1, 10);
    if (!num_queues.has_value()) {
        return std::nullopt;
    }

    std::vector<MlqQueueConfig> queue_configs;
    std::unordered_set<std::string> normalized_qids;

    for (int i = 1; i <= *num_queues; ++i) {
        out_ << "\nQueue " << i << " of " << *num_queues << ":\n";

        // Queue ID
        std::string qid;
        while (true) {
            auto qid_input = read_non_empty_string("  Queue ID (e.g. Q1): ");
            if (!qid_input.has_value()) {
                return std::nullopt;
            }
            std::string norm = ProcessValidator::normalize_id(*qid_input);
            if (normalized_qids.contains(norm)) {
                out_ << "Error: Queue ID '" << *qid_input
                     << "' already exists. Queue IDs must be unique. Please try again.\n";
                continue;
            }
            normalized_qids.insert(norm);
            qid = *qid_input;
            break;
        }

        // Queue priority
        auto qprio = read_int("  Queue priority (integer, smaller = higher priority): ",
                              std::numeric_limits<int>::min(),
                              std::numeric_limits<int>::max());
        if (!qprio.has_value()) {
            return std::nullopt;
        }

        // Queue algorithm
        out_ << "  Select Queue Scheduling Algorithm:\n";
        out_ << "    1. FCFS\n";
        out_ << "    2. SJF (Non-preemptive)\n";
        out_ << "    3. SRTF (Preemptive)\n";
        out_ << "    4. Round Robin\n";
        out_ << "    5. Priority (Non-preemptive)\n";
        out_ << "    6. Priority (Preemptive)\n";
        auto algo_sel = read_int("  Algorithm choice (1-6): ", 1, 6);
        if (!algo_sel.has_value()) {
            return std::nullopt;
        }

        QueueAlgorithm qalgo = QueueAlgorithm::Fcfs;
        Time internal_quantum = 0;
        switch (*algo_sel) {
            case 1: qalgo = QueueAlgorithm::Fcfs; break;
            case 2: qalgo = QueueAlgorithm::Sjf; break;
            case 3: qalgo = QueueAlgorithm::Srtf; break;
            case 4: {
                qalgo = QueueAlgorithm::RoundRobin;
                auto q = read_int64("  Internal Round Robin quantum (> 0): ", 1, std::numeric_limits<std::int64_t>::max());
                if (!q.has_value()) {
                    return std::nullopt;
                }
                internal_quantum = *q;
                break;
            }
            case 5: qalgo = QueueAlgorithm::NonPreemptivePriority; break;
            case 6: qalgo = QueueAlgorithm::PreemptivePriority; break;
            default: break;
        }

        queue_configs.emplace_back(qid, *qprio, qalgo, internal_quantum);
    }

    // Inter-queue policy
    out_ << "\nSelect Inter-Queue Scheduling Policy:\n";
    out_ << "  1. Fixed Priority Preemptive\n";
    out_ << "  2. Round Robin\n";
    auto policy_sel = read_int("Policy choice (1-2): ", 1, 2);
    if (!policy_sel.has_value()) {
        return std::nullopt;
    }

    InterQueuePolicy inter_policy = (*policy_sel == 2)
        ? InterQueuePolicy::RoundRobin
        : InterQueuePolicy::FixedPriorityPreemptive;

    Time default_iq_quantum = 2;
    if (inter_policy == InterQueuePolicy::RoundRobin) {
        auto iq_q = read_int64("Enter inter-queue Round Robin quantum (> 0): ", 1, std::numeric_limits<std::int64_t>::max());
        if (!iq_q.has_value()) {
            return std::nullopt;
        }
        default_iq_quantum = *iq_q;
    }

    // MLQ Workload Processes
    out_ << "\n--- Multilevel Queue Workload Assignment ---\n";
    auto num_procs = read_int("Enter number of processes (1-100): ", 1, 100);
    if (!num_procs.has_value()) {
        return std::nullopt;
    }

    std::vector<MlqProcessBinding> bindings;
    bindings.reserve(static_cast<std::size_t>(*num_procs));
    std::unordered_set<std::string> normalized_pids;

    for (int i = 1; i <= *num_procs; ++i) {
        out_ << "\nProcess " << i << " of " << *num_procs << ":\n";

        // Process ID
        std::string pid;
        while (true) {
            auto id_input = read_non_empty_string("  Process ID (e.g. P1): ");
            if (!id_input.has_value()) {
                return std::nullopt;
            }
            std::string norm_id = ProcessValidator::normalize_id(*id_input);
            if (normalized_pids.contains(norm_id)) {
                out_ << "Error: Process ID '" << *id_input
                     << "' already exists. Process IDs must be unique. Please try again.\n";
                continue;
            }
            normalized_pids.insert(norm_id);
            pid = *id_input;
            break;
        }

        // Arrival time
        auto arrival = read_int64("  Arrival time (>= 0): ", 0, std::numeric_limits<std::int64_t>::max());
        if (!arrival.has_value()) {
            return std::nullopt;
        }

        // Burst time
        auto burst = read_int64("  Burst time (> 0): ", 1, std::numeric_limits<std::int64_t>::max());
        if (!burst.has_value()) {
            return std::nullopt;
        }

        // Process priority
        auto prio = read_int("  Process priority (integer, smaller = higher priority): ",
                             std::numeric_limits<int>::min(),
                             std::numeric_limits<int>::max());
        if (!prio.has_value()) {
            return std::nullopt;
        }

        // Queue assignment
        std::string assigned_qid;
        while (true) {
            out_ << "  Assign to Queue (available: ";
            for (std::size_t qidx = 0; qidx < queue_configs.size(); ++qidx) {
                out_ << queue_configs[qidx].id;
                if (qidx + 1 < queue_configs.size()) {
                    out_ << ", ";
                }
            }
            out_ << "): ";

            auto qid_input = read_line();
            if (!qid_input.has_value()) {
                return std::nullopt;
            }
            if (qid_input->empty()) {
                out_ << "Error: Queue assignment cannot be empty. Please try again.\n";
                continue;
            }

            // Find matching queue (case-insensitive)
            std::string norm_assigned = ProcessValidator::normalize_id(*qid_input);
            auto it = std::find_if(queue_configs.begin(), queue_configs.end(),
                                   [&norm_assigned](const MlqQueueConfig& qc) {
                                       return ProcessValidator::normalize_id(qc.id) == norm_assigned;
                                   });
            if (it == queue_configs.end()) {
                out_ << "Error: Queue '" << *qid_input << "' not found. Please assign to an existing queue.\n";
                continue;
            }

            assigned_qid = it->id;
            break;
        }

        bindings.emplace_back(Process(pid, *arrival, *burst, *prio), assigned_qid);
    }

    return MlqWorkload(std::move(queue_configs),
                       std::move(bindings),
                       inter_policy,
                       default_iq_quantum);
}

std::optional<MlfqConfig> CliController::prompt_mlfq_config() {
    out_ << "\n--- Multilevel Feedback Queue (MLFQ) Configuration ---\n";
    auto num_levels = read_int("Enter number of queue levels (2-10): ", 2, 10);
    if (!num_levels.has_value()) {
        return std::nullopt;
    }

    std::vector<MlfqLevelConfig> levels;
    levels.reserve(static_cast<std::size_t>(*num_levels));

    for (int i = 0; i < *num_levels; ++i) {
        out_ << "\nLevel " << i << " (0 is highest priority):\n";
        auto quantum = read_int64("  Time quantum (> 0): ", 1, std::numeric_limits<std::int64_t>::max());
        if (!quantum.has_value()) {
            return std::nullopt;
        }

        // Custom allotment option
        std::optional<Time> allotment = std::nullopt;
        while (true) {
            out_ << "  Specify custom CPU allotment? (y/n) [default: n]: ";
            auto ans = read_line();
            if (!ans.has_value()) {
                return std::nullopt;
            }
            if (ans->empty() || *ans == "n" || *ans == "N" || *ans == "no") {
                break;
            }
            if (*ans == "y" || *ans == "Y" || *ans == "yes") {
                auto allot = read_int64("  CPU allotment (>= time quantum): ", *quantum, std::numeric_limits<std::int64_t>::max());
                if (!allot.has_value()) {
                    return std::nullopt;
                }
                allotment = *allot;
                break;
            }
            out_ << "Error: Please enter 'y' or 'n'.\n";
        }

        levels.emplace_back(*quantum, allotment);
    }

    // Boost interval
    std::optional<Time> boost_interval = std::nullopt;
    while (true) {
        out_ << "\nEnable periodic priority boosting? (y/n) [default: n]: ";
        auto ans = read_line();
        if (!ans.has_value()) {
            return std::nullopt;
        }
        if (ans->empty() || *ans == "n" || *ans == "N" || *ans == "no") {
            break;
        }
        if (*ans == "y" || *ans == "Y" || *ans == "yes") {
            auto boost = read_int64("  Priority boost interval (> 0): ", 1, std::numeric_limits<std::int64_t>::max());
            if (!boost.has_value()) {
                return std::nullopt;
            }
            boost_interval = *boost;
            break;
        }
        out_ << "Error: Please enter 'y' or 'n'.\n";
    }

    return MlfqConfig(std::move(levels), boost_interval);
}

std::optional<int> CliController::prompt_post_simulation_choice() {
    out_ << "\n------------------------------------------------------------\n";
    out_ << "Options:\n";
    out_ << "  1. Run another simulation\n";
    out_ << "  2. Exit\n";
    out_ << "------------------------------------------------------------\n";
    return read_int("Enter selection (1-2): ", 1, 2);
}

bool CliController::execute_simulation_cycle() {
    auto menu_choice = prompt_menu_choice();
    if (!menu_choice.has_value() || *menu_choice == 9) {
        return false;
    }

    SchedulingAlgorithm algo = SchedulingAlgorithm::Fcfs;
    switch (*menu_choice) {
        case 1: algo = SchedulingAlgorithm::Fcfs; break;
        case 2: algo = SchedulingAlgorithm::Sjf; break;
        case 3: algo = SchedulingAlgorithm::Srtf; break;
        case 4: algo = SchedulingAlgorithm::RoundRobin; break;
        case 5: algo = SchedulingAlgorithm::NonPreemptivePriority; break;
        case 6: algo = SchedulingAlgorithm::PreemptivePriority; break;
        case 7: algo = SchedulingAlgorithm::MultilevelQueue; break;
        case 8: algo = SchedulingAlgorithm::MultilevelFeedbackQueue; break;
        default: return false;
    }

    SchedulingRequest request;

    if (algo == SchedulingAlgorithm::MultilevelQueue) {
        auto mlq_workload = prompt_mlq_workload();
        if (!mlq_workload.has_value()) {
            return false;
        }
        request = SchedulingRequest::mlq(std::move(*mlq_workload));
    } else if (algo == SchedulingAlgorithm::MultilevelFeedbackQueue) {
        auto mlfq_cfg = prompt_mlfq_config();
        if (!mlfq_cfg.has_value()) {
            return false;
        }
        auto procs = prompt_workload(algo);
        if (!procs.has_value()) {
            return false;
        }
        request = SchedulingRequest::mlfq(std::move(*procs), std::move(*mlfq_cfg));
    } else if (algo == SchedulingAlgorithm::RoundRobin) {
        auto rr_cfg = prompt_rr_config();
        if (!rr_cfg.has_value()) {
            return false;
        }
        auto procs = prompt_workload(algo);
        if (!procs.has_value()) {
            return false;
        }
        request = SchedulingRequest::round_robin(std::move(*procs), rr_cfg->time_quantum);
    } else {
        auto procs = prompt_workload(algo);
        if (!procs.has_value()) {
            return false;
        }
        request = SchedulingRequest(algo, std::move(*procs));
    }

    out_ << "\nExecuting simulation...\n";
    ScheduleOutcome outcome = SchedulingEngine::execute(request);

    if (outcome.has_value()) {
        const auto& result = outcome.value();
        bool show_priority = (algo == SchedulingAlgorithm::NonPreemptivePriority ||
                              algo == SchedulingAlgorithm::PreemptivePriority ||
                              algo == SchedulingAlgorithm::MultilevelQueue);
        bool show_queue = (algo == SchedulingAlgorithm::MultilevelQueue);

        format_simulation_summary(out_, request, result);
        format_process_table(out_, result, show_priority, show_queue);
        format_aggregate_metrics(out_, result.metrics());
        format_timeline(out_, result.timeline());
    } else {
        format_error(out_, outcome.error());
    }

    auto post_choice = prompt_post_simulation_choice();
    if (!post_choice.has_value() || *post_choice == 2) {
        return false;
    }

    return true;
}

} // namespace cpu_scheduler
