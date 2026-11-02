#include "cpu_scheduler/scheduling_result.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace cpu_scheduler {

const ProcessState* SchedulingResult::find_process_state(std::string_view process_id) const noexcept {
    const auto it = std::find_if(processes_.begin(), processes_.end(),
                                 [process_id](const ProcessState& ps) {
                                     return ps.process_id == process_id;
                                 });
    return (it != processes_.end()) ? &(*it) : nullptr;
}

ScheduleOutcome::ScheduleOutcome(SchedulingResult result)
    : has_value_(true),
      result_(std::move(result)),
      error_(std::nullopt) {}

ScheduleOutcome::ScheduleOutcome(ScheduleError error)
    : has_value_(false),
      result_(std::nullopt),
      error_(std::move(error)) {}

const SchedulingResult& ScheduleOutcome::value() const {
    if (!has_value_ || !result_.has_value()) {
        throw std::logic_error("Attempted to access value of failed ScheduleOutcome: " +
                               error_message());
    }
    return *result_;
}

const ScheduleError& ScheduleOutcome::error() const {
    if (has_value_ || !error_.has_value()) {
        throw std::logic_error("Attempted to access error of successful ScheduleOutcome");
    }
    return *error_;
}

std::string ScheduleOutcome::error_message() const {
    if (has_value_) {
        return "Success";
    }
    if (!error_.has_value()) {
        return "Unknown error";
    }

    if (error_->validation_result.has_value()) {
        return error_->message + "\n" + error_->validation_result->summary();
    }
    return error_->message;
}

} // namespace cpu_scheduler
