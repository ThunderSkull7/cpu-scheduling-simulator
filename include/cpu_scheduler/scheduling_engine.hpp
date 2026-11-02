#pragma once

#include "cpu_scheduler/fcfs_scheduler.hpp"
#include "cpu_scheduler/mlfq_scheduler.hpp"
#include "cpu_scheduler/mlq_scheduler.hpp"
#include "cpu_scheduler/priority_scheduler.hpp"
#include "cpu_scheduler/process.hpp"
#include "cpu_scheduler/rr_scheduler.hpp"
#include "cpu_scheduler/scheduling_result.hpp"
#include "cpu_scheduler/sjf_scheduler.hpp"
#include "cpu_scheduler/srtf_scheduler.hpp"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cpu_scheduler {

/**
 * @brief Distinct identifiers for all supported CPU scheduling algorithms.
 *
 * Preemptive and non-preemptive variants are strictly distinguished.
 */
enum class SchedulingAlgorithm {
    Fcfs,                    ///< First Come, First Served (Non-preemptive)
    Sjf,                     ///< Shortest Job First (Non-preemptive)
    Srtf,                    ///< Shortest Remaining Time First (Preemptive SJF)
    RoundRobin,              ///< Round Robin (Preemptive, requires time quantum)
    NonPreemptivePriority,   ///< Non-Preemptive Priority
    PreemptivePriority,      ///< Preemptive Priority
    MultilevelQueue,         ///< Multilevel Queue (MLQ, requires MlqWorkload)
    MultilevelFeedbackQueue  ///< Multilevel Feedback Queue (MLFQ, requires MlfqConfig)
};

/**
 * @brief Configuration parameter for Round Robin scheduling.
 */
struct RoundRobinConfig {
    Time time_quantum{1};

    constexpr explicit RoundRobinConfig(Time quantum = 1) noexcept
        : time_quantum(quantum) {}

    [[nodiscard]] bool operator==(const RoundRobinConfig& other) const = default;
};

/**
 * @brief Type-safe variant holding algorithm-specific configuration.
 *
 * Algorithms that require no additional parameters (FCFS, SJF, SRTF, Priority)
 * use std::monostate.
 */
using AlgorithmConfig = std::variant<
    std::monostate,       ///< No configuration (FCFS, SJF, SRTF, Priority)
    RoundRobinConfig,     ///< Round Robin time quantum
    MlqWorkload,          ///< Multilevel Queue workload & queue configuration
    MlfqConfig            ///< Multilevel Feedback Queue tier configuration
>;

/**
 * @brief Encapsulates a complete scheduling execution request.
 */
struct SchedulingRequest {
    SchedulingAlgorithm algorithm{SchedulingAlgorithm::Fcfs};
    std::vector<Process> workload{};
    AlgorithmConfig config{std::monostate{}};

    SchedulingRequest() = default;

    SchedulingRequest(SchedulingAlgorithm algo,
                      std::vector<Process> procs,
                      AlgorithmConfig cfg = std::monostate{})
        : algorithm(algo), workload(std::move(procs)), config(std::move(cfg)) {}

    // Fluent builder methods
    SchedulingRequest& set_algorithm(SchedulingAlgorithm algo) noexcept {
        algorithm = algo;
        return *this;
    }

    SchedulingRequest& set_workload(std::vector<Process> procs) {
        workload = std::move(procs);
        return *this;
    }

    SchedulingRequest& set_config(AlgorithmConfig cfg) {
        config = std::move(cfg);
        return *this;
    }

    // Static convenience factories
    static SchedulingRequest fcfs(std::vector<Process> procs) {
        return {SchedulingAlgorithm::Fcfs, std::move(procs)};
    }

    static SchedulingRequest sjf(std::vector<Process> procs) {
        return {SchedulingAlgorithm::Sjf, std::move(procs)};
    }

    static SchedulingRequest srtf(std::vector<Process> procs) {
        return {SchedulingAlgorithm::Srtf, std::move(procs)};
    }

    static SchedulingRequest round_robin(std::vector<Process> procs, Time quantum) {
        return {SchedulingAlgorithm::RoundRobin, std::move(procs), RoundRobinConfig{quantum}};
    }

    static SchedulingRequest non_preemptive_priority(std::vector<Process> procs) {
        return {SchedulingAlgorithm::NonPreemptivePriority, std::move(procs)};
    }

    static SchedulingRequest preemptive_priority(std::vector<Process> procs) {
        return {SchedulingAlgorithm::PreemptivePriority, std::move(procs)};
    }

    static SchedulingRequest mlq(MlqWorkload mlq_workload) {
        std::vector<Process> procs;
        procs.reserve(mlq_workload.processes.size());
        for (const auto& binding : mlq_workload.processes) {
            procs.push_back(binding.process);
        }
        return {SchedulingAlgorithm::MultilevelQueue, std::move(procs), std::move(mlq_workload)};
    }

    static SchedulingRequest mlfq(std::vector<Process> procs, MlfqConfig mlfq_config) {
        return {SchedulingAlgorithm::MultilevelFeedbackQueue, std::move(procs), std::move(mlfq_config)};
    }

    [[nodiscard]] bool operator==(const SchedulingRequest& other) const = default;
};

// Introspection and reflection helpers
[[nodiscard]] std::string_view to_string(SchedulingAlgorithm algorithm) noexcept;
[[nodiscard]] std::string_view algorithm_name(SchedulingAlgorithm algorithm) noexcept;
[[nodiscard]] std::optional<SchedulingAlgorithm> algorithm_from_string(std::string_view name) noexcept;
[[nodiscard]] std::span<const SchedulingAlgorithm> supported_algorithms() noexcept;

/**
 * @brief Unified scheduling engine and dispatcher.
 *
 * Dispatches scheduling requests to the appropriate underlying scheduler implementation,
 * validating algorithm-specific configurations without duplicating scheduling algorithms.
 */
class SchedulingEngine {
public:
    SchedulingEngine() = default;

    /**
     * @brief Executes the specified scheduling request.
     *
     * Validates that the requested algorithm is valid and that algorithm-specific
     * configuration matches requirements, then dispatches to the corresponding scheduler.
     *
     * @param request The scheduling request to execute.
     * @return ScheduleOutcome containing the SchedulingResult or ScheduleError.
     */
    [[nodiscard]] static ScheduleOutcome execute(const SchedulingRequest& request);

    /**
     * @brief Alias for execute() conforming to scheduler interface convention.
     */
    [[nodiscard]] static ScheduleOutcome schedule(const SchedulingRequest& request) {
        return execute(request);
    }

    /**
     * @brief Convenience overload executing with individual parameters.
     */
    [[nodiscard]] static ScheduleOutcome schedule(
        SchedulingAlgorithm algorithm,
        std::span<const Process> workload,
        const AlgorithmConfig& config = std::monostate{});

    /**
     * @brief Convenience vector overload.
     */
    [[nodiscard]] static ScheduleOutcome schedule(
        SchedulingAlgorithm algorithm,
        const std::vector<Process>& workload,
        const AlgorithmConfig& config = std::monostate{}) {
        return schedule(algorithm, std::span<const Process>(workload), config);
    }
};

using UnifiedScheduler = SchedulingEngine;

} // namespace cpu_scheduler
