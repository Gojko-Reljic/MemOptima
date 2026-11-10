#include <memoptima/application_info.hpp>
#include <memoptima/analysis/memory_pressure.hpp>
#include <memoptima/analysis/memory_status.hpp>
#include <memoptima/linux/memory_reader.hpp>
#include <memoptima/linux/process_scanner.hpp>

#include "cli/processes_formatter.hpp"
#include "cli/status_formatter.hpp"

#include <iostream>
#include <string_view>

namespace {

constexpr int kExitSuccess = 0;
constexpr int kExitRuntimeError = 1;
constexpr int kExitUsageError = 2;

void print_help(std::ostream& output) {
    output << memoptima::application_name() << " — " << memoptima::application_description() << "\n\n"
           << "Usage:\n"
           << "  memoptima [OPTION]\n\n"
           << "Options:\n"
           << "  -h, --help       Display this help message.\n"
           << "  processes        Display the top memory-consuming processes.\n"
           << "  status           Display current RAM and swap status.\n"
           << "  -v, --version    Display the application version.\n";
}

void print_memory_read_error(const memoptima::linux::MemoryReadError& error) {
    std::cerr << "Error: unable to read system memory information:\n";

    if (!error.metric_name.empty()) {
        std::cerr << error.metric_name;
        if (error.line_number != 0) {
            std::cerr << " on line " << error.line_number;
        }
        std::cerr << ": ";
    }

    std::cerr << error.detail << '\n';
}

int run_status_command() {
    const auto snapshot = memoptima::linux::read_system_memory_snapshot();
    if (!snapshot.has_value()) {
        print_memory_read_error(snapshot.error());
        return kExitRuntimeError;
    }

    const memoptima::analysis::MemoryStatus status = memoptima::analysis::evaluate_memory_status(*snapshot);
    const memoptima::analysis::MemoryPressure pressure = memoptima::analysis::evaluate_memory_pressure(status);
    std::cout << memoptima::cli::format_memory_status(status, pressure);
    return kExitSuccess;
}

int run_processes_command() {
    const auto scan_result = memoptima::linux::scan_process_memory();
    if (!scan_result.has_value()) {
        std::cerr << "Error: unable to scan process memory information: " << scan_result.error().detail << '\n';
        return kExitRuntimeError;
    }

    std::cout << memoptima::cli::format_process_scan_result(*scan_result);
    return kExitSuccess;
}

int run(int argument_count, char* arguments[]) {
    if (argument_count == 1) {
        print_help(std::cout);
        return kExitSuccess;
    }

    if (argument_count != 2) {
        std::cerr << "Error: expected at most one option.\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }

    const std::string_view option{arguments[1]};

    if (option == "status") {
        return run_status_command();
    }

    if (option == "processes") {
        return run_processes_command();
    }

    if (option == "-h" || option == "--help") {
        print_help(std::cout);
        return kExitSuccess;
    }

    if (option == "-v" || option == "--version") {
        std::cout << "memoptima " << memoptima::version() << '\n';
        return kExitSuccess;
    }

    std::cerr << "Error: unknown option '" << option << "'.\n\n";
    print_help(std::cerr);
    return kExitUsageError;
}

}  // namespace

int main(int argument_count, char* arguments[]) {
    return run(argument_count, arguments);
}
