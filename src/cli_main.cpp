#include "cpu_scheduler/cli.hpp"

#include <iostream>

int main() {
    cpu_scheduler::CliController cli(std::cin, std::cout);
    return cli.run();
}
