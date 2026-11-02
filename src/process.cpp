#include "cpu_scheduler/process.hpp"

#include <iostream>

namespace cpu_scheduler {

std::ostream& operator<<(std::ostream& os, const Process& process) {
    os << "Process{id: \"" << process.id
       << "\", arrival_time: " << process.arrival_time
       << ", burst_time: " << process.burst_time
       << ", priority: " << process.priority << "}";
    return os;
}

} // namespace cpu_scheduler
