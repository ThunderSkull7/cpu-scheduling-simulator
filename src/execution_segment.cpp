#include "cpu_scheduler/execution_segment.hpp"

#include <iostream>

namespace cpu_scheduler {

std::ostream& operator<<(std::ostream& os, SegmentType type) {
    switch (type) {
        case SegmentType::Process:
            os << "Process";
            break;
        case SegmentType::Idle:
            os << "Idle";
            break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const ExecutionSegment& segment) {
    os << "ExecutionSegment{type: " << segment.type << ", id: ";
    if (segment.process_id.has_value()) {
        os << "\"" << *segment.process_id << "\"";
    } else {
        os << "null";
    }
    os << ", start: " << segment.start_time
       << ", end: " << segment.end_time
       << ", duration: " << segment.duration() << "}";
    return os;
}

} // namespace cpu_scheduler
