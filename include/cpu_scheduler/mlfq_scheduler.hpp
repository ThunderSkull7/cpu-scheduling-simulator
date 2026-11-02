#pragma once

#include "cpu_scheduler/process.hpp"
#include "cpu_scheduler/scheduling_result.hpp"

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cpu_scheduler {

/**
 * @brief Configuration for an individual queue level in the MLFQ scheduler.
 */
struct MlfqLevelConfig {
    Time time_quantum{2};
    std::optional<Time> cpu_allotment{std::nullopt}; // Defaults to time_quantum if not specified

    constexpr MlfqLevelConfig() = default;
    constexpr explicit MlfqLevelConfig(Time quantum, std::optional<Time> allotment = std::nullopt)
        : time_quantum(quantum), cpu_allotment(allotment) {}

    [[nodiscard]] Time effective_allotment() const noexcept {
        return cpu_allotment.value_or(time_quantum);
    }

    [[nodiscard]] bool operator==(const MlfqLevelConfig& other) const = default;
};

/**
 * @brief Global configuration for the Multilevel Feedback Queue (MLFQ) scheduler.
 */
struct MlfqConfig {
    std::vector<MlfqLevelConfig> levels{};
    std::optional<Time> boost_interval{std::nullopt}; // std::nullopt or 0 disables boosting

    MlfqConfig() = default;

    explicit MlfqConfig(std::vector<MlfqLevelConfig> queue_levels,
                        std::optional<Time> boost = std::nullopt)
        : levels(std::move(queue_levels)), boost_interval(boost) {}

    MlfqConfig(std::initializer_list<Time> quantums, std::optional<Time> boost = std::nullopt)
        : boost_interval(boost) {
        levels.reserve(quantums.size());
        for (Time q : quantums) {
            levels.emplace_back(q);
        }
    }

    [[nodiscard]] bool is_boost_enabled() const noexcept {
        return boost_interval.has_value() && *boost_interval > 0;
    }

    MlfqConfig& add_level(Time time_quantum, std::optional<Time> allotment = std::nullopt) {
        levels.emplace_back(time_quantum, allotment);
        return *this;
    }

    MlfqConfig& set_boost_interval(std::optional<Time> interval) noexcept {
        boost_interval = interval;
        return *this;
    }

    [[nodiscard]] bool operator==(const MlfqConfig& other) const = default;
};

/**
 * @brief Multilevel Feedback Queue (MLFQ) CPU Scheduler.
 *
 * Implements dynamic process feedback across multiple priority queue levels:
 *  - Queue 0 has highest priority; queue L-1 has lowest.
 *  - All newly arrived processes automatically enter Queue 0.
 *  - Each queue operates Round Robin with a level-specific time quantum.
 *  - Per-process CPU consumption at the current level is tracked across interruptions.
 *  - A process consuming its full level allotment is demoted to the next lower level.
 *  - When ready processes exist at a higher-priority level, lower-level processes are preempted.
 *  - Periodic global priority boosting moves all unfinished processes to Queue 0 to prevent starvation.
 */
class MlfqScheduler {
public:
    MlfqScheduler() = default;
    explicit MlfqScheduler(MlfqConfig config) : config_(std::move(config)) {}

    [[nodiscard]] const MlfqConfig& config() const noexcept { return config_; }
    void set_config(MlfqConfig config) noexcept { config_ = std::move(config); }

    [[nodiscard]] ScheduleOutcome schedule(std::span<const Process> processes) const;
    [[nodiscard]] ScheduleOutcome schedule(const std::vector<Process>& processes) const {
        return schedule(std::span<const Process>(processes));
    }

    [[nodiscard]] static ScheduleOutcome run(std::span<const Process> processes, const MlfqConfig& config) {
        MlfqScheduler scheduler(config);
        return scheduler.schedule(processes);
    }

    [[nodiscard]] static ScheduleOutcome run(const std::vector<Process>& processes, const MlfqConfig& config) {
        return run(std::span<const Process>(processes), config);
    }

private:
    MlfqConfig config_{};
};

} // namespace cpu_scheduler
