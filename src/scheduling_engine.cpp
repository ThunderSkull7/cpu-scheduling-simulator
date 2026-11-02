#include "cpu_scheduler/scheduling_engine.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace cpu_scheduler {

namespace {

constexpr std::array<SchedulingAlgorithm, 8> kSupportedAlgorithms = {
    SchedulingAlgorithm::Fcfs,
    SchedulingAlgorithm::Sjf,
    SchedulingAlgorithm::Srtf,
    SchedulingAlgorithm::RoundRobin,
    SchedulingAlgorithm::NonPreemptivePriority,
    SchedulingAlgorithm::PreemptivePriority,
    SchedulingAlgorithm::MultilevelQueue,
    SchedulingAlgorithm::MultilevelFeedbackQueue
};

std::string normalize_string(std::string_view sv) {
    std::string result;
    result.reserve(sv.size());
    for (char c : sv) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return result;
}

} // namespace

std::string_view to_string(SchedulingAlgorithm algorithm) noexcept {
    switch (algorithm) {
        case SchedulingAlgorithm::Fcfs: return "FCFS";
        case SchedulingAlgorithm::Sjf: return "SJF";
        case SchedulingAlgorithm::Srtf: return "SRTF";
        case SchedulingAlgorithm::RoundRobin: return "RoundRobin";
        case SchedulingAlgorithm::NonPreemptivePriority: return "NonPreemptivePriority";
        case SchedulingAlgorithm::PreemptivePriority: return "PreemptivePriority";
        case SchedulingAlgorithm::MultilevelQueue: return "MLQ";
        case SchedulingAlgorithm::MultilevelFeedbackQueue: return "MLFQ";
    }
    return "Unknown";
}

std::string_view algorithm_name(SchedulingAlgorithm algorithm) noexcept {
    switch (algorithm) {
        case SchedulingAlgorithm::Fcfs: return "First Come, First Served (FCFS)";
        case SchedulingAlgorithm::Sjf: return "Shortest Job First (SJF, Non-Preemptive)";
        case SchedulingAlgorithm::Srtf: return "Shortest Remaining Time First (SRTF, Preemptive)";
        case SchedulingAlgorithm::RoundRobin: return "Round Robin (RR)";
        case SchedulingAlgorithm::NonPreemptivePriority: return "Non-Preemptive Priority";
        case SchedulingAlgorithm::PreemptivePriority: return "Preemptive Priority";
        case SchedulingAlgorithm::MultilevelQueue: return "Multilevel Queue (MLQ)";
        case SchedulingAlgorithm::MultilevelFeedbackQueue: return "Multilevel Feedback Queue (MLFQ)";
    }
    return "Unknown";
}

std::optional<SchedulingAlgorithm> algorithm_from_string(std::string_view name) noexcept {
    const std::string norm = normalize_string(name);

    if (norm == "fcfs" || norm == "firstcomefirstserved") {
        return SchedulingAlgorithm::Fcfs;
    }
    if (norm == "sjf" || norm == "shortestjobfirst") {
        return SchedulingAlgorithm::Sjf;
    }
    if (norm == "srtf" || norm == "shortestremainingtimefirst" || norm == "preemptivesjf") {
        return SchedulingAlgorithm::Srtf;
    }
    if (norm == "rr" || norm == "roundrobin") {
        return SchedulingAlgorithm::RoundRobin;
    }
    if (norm == "nonpreemptivepriority" || norm == "nppriority" || norm == "prioritynp") {
        return SchedulingAlgorithm::NonPreemptivePriority;
    }
    if (norm == "preemptivepriority" || norm == "ppriority" || norm == "priorityp" || norm == "priority") {
        return SchedulingAlgorithm::PreemptivePriority;
    }
    if (norm == "mlq" || norm == "multilevelqueue") {
        return SchedulingAlgorithm::MultilevelQueue;
    }
    if (norm == "mlfq" || norm == "multilevelfeedbackqueue") {
        return SchedulingAlgorithm::MultilevelFeedbackQueue;
    }

    return std::nullopt;
}

std::span<const SchedulingAlgorithm> supported_algorithms() noexcept {
    return kSupportedAlgorithms;
}

ScheduleOutcome SchedulingEngine::execute(const SchedulingRequest& request) {
    // ------------------------------------------------------------------------
    // 1. Validate Algorithm Identifier
    // ------------------------------------------------------------------------
    const auto it = std::find(kSupportedAlgorithms.begin(), kSupportedAlgorithms.end(), request.algorithm);
    if (it == kSupportedAlgorithms.end()) {
        return ScheduleError{
            .kind = ScheduleErrorKind::ValidationError,
            .message = "Scheduling engine error: unknown or unsupported scheduling algorithm identifier (" +
                       std::to_string(static_cast<int>(request.algorithm)) + ").",
            .validation_result = std::nullopt
        };
    }

    // ------------------------------------------------------------------------
    // 2. Validate Configuration Compatibility
    // ------------------------------------------------------------------------
    switch (request.algorithm) {
        case SchedulingAlgorithm::Fcfs:
        case SchedulingAlgorithm::Sjf:
        case SchedulingAlgorithm::Srtf:
        case SchedulingAlgorithm::NonPreemptivePriority:
        case SchedulingAlgorithm::PreemptivePriority: {
            if (!std::holds_alternative<std::monostate>(request.config)) {
                return ScheduleError{
                    .kind = ScheduleErrorKind::ValidationError,
                    .message = "Scheduling engine error: irrelevant configuration supplied for algorithm '" +
                               std::string(algorithm_name(request.algorithm)) +
                               "' (algorithm takes no configuration parameters).",
                    .validation_result = std::nullopt
                };
            }
            break;
        }

        case SchedulingAlgorithm::RoundRobin: {
            if (std::holds_alternative<std::monostate>(request.config)) {
                return ScheduleError{
                    .kind = ScheduleErrorKind::ValidationError,
                    .message = "Scheduling engine error: missing configuration for Round Robin (time quantum must be provided via RoundRobinConfig).",
                    .validation_result = std::nullopt
                };
            }
            if (!std::holds_alternative<RoundRobinConfig>(request.config)) {
                return ScheduleError{
                    .kind = ScheduleErrorKind::ValidationError,
                    .message = "Scheduling engine error: incompatible configuration type for Round Robin (expected RoundRobinConfig).",
                    .validation_result = std::nullopt
                };
            }
            const auto& rr_cfg = std::get<RoundRobinConfig>(request.config);
            if (rr_cfg.time_quantum <= 0) {
                return ScheduleError{
                    .kind = ScheduleErrorKind::ValidationError,
                    .message = "Scheduling engine error: Round Robin time quantum must be positive (> 0), got " +
                               std::to_string(rr_cfg.time_quantum) + ".",
                    .validation_result = std::nullopt
                };
            }
            break;
        }

        case SchedulingAlgorithm::MultilevelQueue: {
            if (std::holds_alternative<std::monostate>(request.config)) {
                return ScheduleError{
                    .kind = ScheduleErrorKind::ValidationError,
                    .message = "Scheduling engine error: missing configuration for Multilevel Queue (MlqWorkload must be provided).",
                    .validation_result = std::nullopt
                };
            }
            if (!std::holds_alternative<MlqWorkload>(request.config)) {
                return ScheduleError{
                    .kind = ScheduleErrorKind::ValidationError,
                    .message = "Scheduling engine error: incompatible configuration type for Multilevel Queue (expected MlqWorkload).",
                    .validation_result = std::nullopt
                };
            }
            break;
        }

        case SchedulingAlgorithm::MultilevelFeedbackQueue: {
            if (std::holds_alternative<std::monostate>(request.config)) {
                return ScheduleError{
                    .kind = ScheduleErrorKind::ValidationError,
                    .message = "Scheduling engine error: missing configuration for Multilevel Feedback Queue (MlfqConfig must be provided).",
                    .validation_result = std::nullopt
                };
            }
            if (!std::holds_alternative<MlfqConfig>(request.config)) {
                return ScheduleError{
                    .kind = ScheduleErrorKind::ValidationError,
                    .message = "Scheduling engine error: incompatible configuration type for Multilevel Feedback Queue (expected MlfqConfig).",
                    .validation_result = std::nullopt
                };
            }
            break;
        }
    }

    // ------------------------------------------------------------------------
    // 3. Dispatch to Selected Scheduler
    // ------------------------------------------------------------------------
    const std::span<const Process> workload_span(request.workload);

    switch (request.algorithm) {
        case SchedulingAlgorithm::Fcfs:
            return FcfsScheduler::run(workload_span);

        case SchedulingAlgorithm::Sjf:
            return SjfScheduler::run(workload_span);

        case SchedulingAlgorithm::Srtf:
            return SrtfScheduler::run(workload_span);

        case SchedulingAlgorithm::RoundRobin:
            return RrScheduler::run(workload_span, std::get<RoundRobinConfig>(request.config).time_quantum);

        case SchedulingAlgorithm::NonPreemptivePriority:
            return NonPreemptivePriorityScheduler::run(workload_span);

        case SchedulingAlgorithm::PreemptivePriority:
            return PreemptivePriorityScheduler::run(workload_span);

        case SchedulingAlgorithm::MultilevelQueue:
            return MlqScheduler::run(std::get<MlqWorkload>(request.config));

        case SchedulingAlgorithm::MultilevelFeedbackQueue:
            return MlfqScheduler::run(workload_span, std::get<MlfqConfig>(request.config));
    }

    return ScheduleError{
        .kind = ScheduleErrorKind::ValidationError,
        .message = "Scheduling engine error: unexpected dispatcher routing failure.",
        .validation_result = std::nullopt
    };
}

ScheduleOutcome SchedulingEngine::schedule(
    SchedulingAlgorithm algorithm,
    std::span<const Process> workload,
    const AlgorithmConfig& config) {

    std::vector<Process> procs(workload.begin(), workload.end());
    const SchedulingRequest request(algorithm, std::move(procs), config);
    return execute(request);
}

} // namespace cpu_scheduler
