#pragma once

#include "cpu_scheduler/process.hpp"

#include <iosfwd>
#include <optional>
#include <string>

namespace cpu_scheduler {

/// Classification of timeline execution segments.
enum class SegmentType {
    Process,
    Idle
};

/**
 * @brief Represents a continuous slice of CPU execution on the timeline.
 */
struct ExecutionSegment {
    SegmentType type{SegmentType::Process};
    std::optional<std::string> process_id{std::nullopt};
    Time start_time{0};
    Time end_time{0};

    constexpr ExecutionSegment() = default;

    ExecutionSegment(SegmentType seg_type, std::optional<std::string> pid, Time start, Time end)
        : type(seg_type),
          process_id(std::move(pid)),
          start_time(start),
          end_time(end) {}

    static ExecutionSegment process(std::string pid, Time start, Time end) {
        return ExecutionSegment(SegmentType::Process, std::move(pid), start, end);
    }

    static ExecutionSegment idle(Time start, Time end) {
        return ExecutionSegment(SegmentType::Idle, std::nullopt, start, end);
    }

    [[nodiscard]] Time duration() const noexcept {
        return end_time - start_time;
    }

    [[nodiscard]] bool is_idle() const noexcept {
        return type == SegmentType::Idle;
    }

    [[nodiscard]] bool is_process() const noexcept {
        return type == SegmentType::Process;
    }

    [[nodiscard]] bool operator==(const ExecutionSegment& other) const = default;
};

std::ostream& operator<<(std::ostream& os, SegmentType type);
std::ostream& operator<<(std::ostream& os, const ExecutionSegment& segment);

} // namespace cpu_scheduler
