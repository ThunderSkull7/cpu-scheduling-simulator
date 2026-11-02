#pragma once

#include "cpu_scheduler/process.hpp"
#include "cpu_scheduler/scheduling_result.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cpu_scheduler {

/**
 * @brief Algorithms supported for scheduling processes within an individual MLQ queue.
 */
enum class QueueAlgorithm {
    Fcfs,
    Sjf,                    // Non-preemptive Shortest Job First
    Srtf,                   // Preemptive Shortest Remaining Time First
    RoundRobin,             // Round Robin with queue-level quantum
    NonPreemptivePriority,  // Non-preemptive Priority (process priority: smaller = higher)
    PreemptivePriority      // Preemptive Priority (process priority: smaller = higher)
};

/**
 * @brief Policies for allocating CPU time across ready queues.
 */
enum class InterQueuePolicy {
    FixedPriorityPreemptive, // Higher queue priority (smaller numerical value) preempts lower queue priority
    RoundRobin               // Cyclic allocation among non-empty queues with time quantum
};

/**
 * @brief Configuration specification for an individual MLQ scheduling queue.
 */
struct MlqQueueConfig {
    std::string id{};
    int priority{0};                          // Smaller number = strictly higher queue priority
    QueueAlgorithm algorithm{QueueAlgorithm::Fcfs};
    Time internal_quantum{0};                 // Required if algorithm == RoundRobin (> 0)
    Time inter_queue_quantum{0};              // Optional override for InterQueuePolicy::RoundRobin (> 0)

    constexpr MlqQueueConfig() = default;

    MlqQueueConfig(std::string queue_id,
                   int queue_prio,
                   QueueAlgorithm algo,
                   Time internal_q = 0,
                   Time inter_q = 0)
        : id(std::move(queue_id)),
          priority(queue_prio),
          algorithm(algo),
          internal_quantum(internal_q),
          inter_queue_quantum(inter_q) {}

    [[nodiscard]] bool operator==(const MlqQueueConfig& other) const = default;
};

/**
 * @brief Associates an immutable Process with an assigned queue ID.
 */
struct MlqProcessBinding {
    Process process{};
    std::string queue_id{};

    constexpr MlqProcessBinding() = default;

    MlqProcessBinding(Process proc, std::string qid)
        : process(std::move(proc)), queue_id(std::move(qid)) {}

    [[nodiscard]] bool operator==(const MlqProcessBinding& other) const = default;
};

/**
 * @brief Complete workload and queue configuration for Multilevel Queue simulation.
 */
struct MlqWorkload {
    std::vector<MlqQueueConfig> queues{};
    std::vector<MlqProcessBinding> processes{};
    InterQueuePolicy inter_queue_policy{InterQueuePolicy::FixedPriorityPreemptive};
    Time default_inter_queue_quantum{2};

    MlqWorkload() = default;

    MlqWorkload(std::vector<MlqQueueConfig> q_configs,
                std::vector<MlqProcessBinding> procs,
                InterQueuePolicy policy = InterQueuePolicy::FixedPriorityPreemptive,
                Time default_iq_quantum = 2)
        : queues(std::move(q_configs)),
          processes(std::move(procs)),
          inter_queue_policy(policy),
          default_inter_queue_quantum(default_iq_quantum) {}

    MlqWorkload& add_queue(MlqQueueConfig config) {
        queues.push_back(std::move(config));
        return *this;
    }

    MlqWorkload& add_process(Process proc, std::string qid) {
        processes.emplace_back(std::move(proc), std::move(qid));
        return *this;
    }

    MlqWorkload& set_inter_queue_policy(InterQueuePolicy policy) noexcept {
        inter_queue_policy = policy;
        return *this;
    }

    MlqWorkload& set_default_inter_queue_quantum(Time q) noexcept {
        default_inter_queue_quantum = q;
        return *this;
    }
};

/**
 * @brief Multilevel Queue (MLQ) CPU Scheduler.
 *
 * Simulates CPU scheduling across multiple distinct priority queues:
 *  - Each process is permanently bound to exactly one queue (no migration/aging).
 *  - Each queue executes its assigned processes according to its configured algorithm.
 *  - Inter-queue scheduling governs CPU allocation among ready queues using either
 *    Fixed-Priority Preemption or Round Robin time-slicing.
 */
class MlqScheduler {
public:
    constexpr MlqScheduler() = default;

    /**
     * @brief Executes Multilevel Queue scheduling on the provided workload and queue configuration.
     *
     * Validates queue configuration and process bindings before simulation.
     *
     * @param workload Input MLQ workload specification.
     * @return ScheduleOutcome containing the SchedulingResult on success,
     *         or a detailed ScheduleError on validation or arithmetic overflow failure.
     */
    [[nodiscard]] ScheduleOutcome schedule(const MlqWorkload& workload) const;

    /// Static convenience method
    [[nodiscard]] static ScheduleOutcome run(const MlqWorkload& workload) {
        return MlqScheduler{}.schedule(workload);
    }
};

} // namespace cpu_scheduler

