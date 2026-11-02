#include "cpu_scheduler/process_state.hpp"

#include <iostream>

namespace cpu_scheduler {

ProcessState ProcessState::from_process(const Process& process) {
    ProcessState state;
    state.process_id = process.id;
    state.arrival_time = process.arrival_time;
    state.burst_time = process.burst_time;
    state.priority = process.priority;
    state.remaining_time = process.burst_time;
    state.status = ProcessExecutionStatus::NotStarted;
    return state;
}

std::ostream& operator<<(std::ostream& os, ProcessExecutionStatus status) {
    switch (status) {
        case ProcessExecutionStatus::NotStarted:
            os << "NotStarted";
            break;
        case ProcessExecutionStatus::Running:
            os << "Running";
            break;
        case ProcessExecutionStatus::Completed:
            os << "Completed";
            break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const ProcessState& state) {
    os << "ProcessState{id: \"" << state.process_id
       << "\", arrival: " << state.arrival_time
       << ", burst: " << state.burst_time
       << ", start: ";
    if (state.start_time.has_value()) {
        os << *state.start_time;
    } else {
        os << "N/A";
    }
    os << ", completion: " << state.completion_time
       << ", TAT: " << state.turnaround_time
       << ", WT: " << state.waiting_time
       << ", RT: " << state.response_time
       << ", status: " << state.status << "}";
    return os;
}

} // namespace cpu_scheduler
