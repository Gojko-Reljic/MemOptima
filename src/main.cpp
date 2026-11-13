#include <memoptima/analysis/memory_pressure.hpp>
#include <memoptima/analysis/memory_status.hpp>
#include <memoptima/application_info.hpp>
#include <memoptima/docker_container_assessment.hpp>
#include <memoptima/evidence_inspector.hpp>
#include <memoptima/linux/cgroup_v2_capability_checker.hpp>
#include <memoptima/linux/docker_container_scanner.hpp>
#include <memoptima/linux/memory_reader.hpp>
#include <memoptima/linux/process_scanner.hpp>
#include <memoptima/sampling/sampling_configuration.hpp>
#include <memoptima/sampling/session_runner.hpp>

#include "cli/assessments_formatter.hpp"
#include "cli/containers_formatter.hpp"
#include "cli/monitor_runner.hpp"
#include "cli/processes_formatter.hpp"
#include "cli/status_formatter.hpp"

#include <chrono>
#include <csignal>
#include <expected>
#include <filesystem>
#include <iostream>
#include <optional>
#include <ranges>
#include <string_view>
#include <thread>

namespace {

constexpr int kExitSuccess = 0;
constexpr int kExitRuntimeError = 1;
constexpr int kExitUsageError = 2;

// A signal handler only sets this signal-safe flag; normal code performs all cleanup.
volatile std::sig_atomic_t monitor_stop_requested = 0;
volatile std::sig_atomic_t sampling_stop_requested = 0;

void request_monitor_stop(int) noexcept { monitor_stop_requested = 1; }
void request_sampling_stop(int) noexcept { sampling_stop_requested = 1; }

void print_help(std::ostream &output) {
    output << memoptima::application_name() << " — " << memoptima::application_description()
           << "\n\n"
           << "Usage:\n"
           << "  memoptima [OPTION]\n"
           << "  memoptima container <FULL_ID>\n\n"
           << "  memoptima assess containers\n"
           << "  memoptima assess container <FULL_ID>\n"
           << "  memoptima inspect evidence <FILE>\n"
           << "  memoptima sample containers --count <COUNT> --interval <DURATION> --output "
              "<NEW_FILE>\n\n"
           << "Options:\n"
           << "  -h, --help       Display this help message.\n"
           << "  assess           Assess Docker container memory-boundary and OOM signals.\n"
           << "  containers       Display memory information for Docker containers.\n"
           << "  monitor          Continuously display memory status every five seconds.\n"
           << "  inspect          Validate and summarize a local evidence artifact.\n"
           << "  processes        Display the top memory-consuming processes.\n"
           << "  sample           Collect bounded Docker cgroup memory evidence.\n"
           << "  status           Display current RAM and swap status.\n"
           << "  -v, --version    Display the application version.\n";
}

[[nodiscard]] std::string
format_memory_read_error_detail(const memoptima::linux::MemoryReadError &error) {
    std::string detail;

    if (!error.metric_name.empty()) {
        detail += error.metric_name;
        if (error.line_number != 0) {
            detail += " on line ";
            detail += std::to_string(error.line_number);
        }
        detail += ": ";
    }

    detail += error.detail;
    return detail;
}

[[nodiscard]] memoptima::cli::FormattedStatusResult read_formatted_memory_status() {
    const auto snapshot = memoptima::linux::read_system_memory_snapshot();
    if (!snapshot.has_value()) {
        return std::unexpected(format_memory_read_error_detail(snapshot.error()));
    }

    const memoptima::analysis::MemoryStatus status =
        memoptima::analysis::evaluate_memory_status(*snapshot);
    const memoptima::analysis::MemoryPressure pressure =
        memoptima::analysis::evaluate_memory_pressure(status);
    return memoptima::cli::format_memory_status(status, pressure);
}

int run_status_command() {
    const auto formatted_status = read_formatted_memory_status();
    if (!formatted_status.has_value()) {
        std::cerr << "Error: unable to read system memory information:\n"
                  << formatted_status.error() << '\n';
        return kExitRuntimeError;
    }

    std::cout << *formatted_status;
    return kExitSuccess;
}

int run_monitor_command() {
    monitor_stop_requested = 0;
    const auto previous_signal_handler = std::signal(SIGINT, request_monitor_stop);
    if (previous_signal_handler == SIG_ERR) {
        std::cerr << "Error: unable to register the monitor interrupt handler.\n";
        return kExitRuntimeError;
    }

    const memoptima::cli::MonitorDependencies dependencies{
        .current_utc_time = [] { return std::chrono::system_clock::now(); },
        .current_steady_time = [] { return std::chrono::steady_clock::now(); },
        .read_formatted_status =
            [] {
                const auto formatted_status = read_formatted_memory_status();
                if (!formatted_status.has_value()) {
                    return memoptima::cli::FormattedStatusResult{std::unexpected(
                        "unable to read system memory information: " + formatted_status.error())};
                }

                return formatted_status;
            },
        .sleep_for =
            [](std::chrono::milliseconds duration) { std::this_thread::sleep_for(duration); },
        .stop_requested = [] { return monitor_stop_requested != 0; },
    };

    memoptima::cli::run_monitor(dependencies, std::cout, std::cerr);
    // Restore the process-wide SIGINT behavior after the monitor exits.
    std::signal(SIGINT, previous_signal_handler);
    return kExitSuccess;
}

int run_inspect_evidence_command(std::string_view path_text) {
    const auto summary = memoptima::inspect_evidence_artifact(std::filesystem::path{path_text});
    if (!summary.has_value()) {
        std::cerr << "Error: unable to inspect evidence artifact: " << summary.error().detail
                  << '\n';
        return kExitRuntimeError;
    }
    std::cout << "Evidence artifact\n\n"
              << "Path: " << summary->artifact_path.string() << '\n'
              << "Application version: " << summary->application_version << '\n'
              << "Requested samples: " << summary->requested_sample_count << '\n'
              << "Completed samples: " << summary->completed_sample_count << '\n'
              << "Completion: " << (summary->incomplete ? "Incomplete" : summary->terminal_outcome)
              << '\n'
              << "Discovery warnings: " << summary->discovery_warning_count << '\n'
              << "Skipped containers: " << summary->skipped_container_count << "\n\n"
              << "Containers\n";
    if (summary->containers.empty())
        std::cout << "  None\n";
    for (const auto &container : summary->containers)
        std::cout << "  " << container.container_id << "  samples=" << container.samples_seen
                  << " highest_risk=" << container.highest_risk
                  << " partial_samples=" << container.partial_coverage_samples
                  << " last=" << container.last_risk << '/' << container.last_coverage << '\n';
    if (summary->incomplete)
        std::cout << "\nWarning: Incomplete: terminal session_end record is missing.\n";
    return kExitSuccess;
}

struct SampleContainersArguments {
    memoptima::sampling::SamplingCount count;
    memoptima::sampling::SamplingInterval interval;
    std::filesystem::path output_path;
};

[[nodiscard]] std::expected<SampleContainersArguments, std::string>
parse_sample_containers_arguments(int argument_count, char *arguments[]) {
    if (argument_count != 9) {
        return std::unexpected(
            "'sample containers' requires --count, --interval, and --output exactly once.");
    }

    std::optional<std::string_view> count_text;
    std::optional<std::string_view> interval_text;
    std::optional<std::string_view> output_text;
    for (int index = 3; index < argument_count; index += 2) {
        const std::string_view option{arguments[index]};
        const std::string_view value{arguments[index + 1]};
        if (option == "--count") {
            if (count_text.has_value()) {
                return std::unexpected("'sample containers' received --count more than once.");
            }
            count_text = value;
        } else if (option == "--interval") {
            if (interval_text.has_value()) {
                return std::unexpected("'sample containers' received --interval more than once.");
            }
            interval_text = value;
        } else if (option == "--output") {
            if (output_text.has_value()) {
                return std::unexpected("'sample containers' received --output more than once.");
            }
            output_text = value;
        } else {
            return std::unexpected("'sample containers' received unknown option '" +
                                   std::string{option} + "'.");
        }
    }

    if (!count_text.has_value() || !interval_text.has_value() || !output_text.has_value()) {
        return std::unexpected(
            "'sample containers' requires --count, --interval, and --output exactly once.");
    }
    const auto count = memoptima::sampling::parse_sampling_count(*count_text);
    if (!count.has_value()) {
        return std::unexpected("invalid --count: " + count.error().detail);
    }
    const auto interval = memoptima::sampling::parse_sampling_interval(*interval_text);
    if (!interval.has_value()) {
        return std::unexpected("invalid --interval: " + interval.error().detail);
    }
    return SampleContainersArguments{
        .count = *count,
        .interval = *interval,
        .output_path = std::filesystem::path{*output_text},
    };
}

int run_sample_containers_command(int argument_count, char *arguments[]) {
    const auto parsed_arguments = parse_sample_containers_arguments(argument_count, arguments);
    if (!parsed_arguments.has_value()) {
        std::cerr << "Error: " << parsed_arguments.error() << "\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }

    std::optional<memoptima::linux::CgroupV2Environment> environment;
    sampling_stop_requested = 0;
    const auto previous_signal_handler = std::signal(SIGINT, request_sampling_stop);
    if (previous_signal_handler == SIG_ERR) {
        std::cerr << "Error: unable to register the sampling interrupt handler.\n";
        return kExitRuntimeError;
    }

    const memoptima::sampling::SamplingSessionDependencies dependencies{
        .preflight = [&environment] -> std::expected<void, std::string> {
            const auto detected_environment = memoptima::linux::detect_cgroup_v2_environment();
            if (!detected_environment.has_value()) {
                return std::unexpected(detected_environment.error().detail);
            }
            environment = *detected_environment;
            return {};
        },
        .scan_containers =
            [&environment] {
                return memoptima::linux::scan_rootful_docker_containers(*environment);
            },
        .current_utc_time = [] { return std::chrono::system_clock::now(); },
        .current_steady_time = [] { return std::chrono::steady_clock::now(); },
        .sleep_for =
            [](std::chrono::milliseconds duration) { std::this_thread::sleep_for(duration); },
        .stop_requested = [] { return sampling_stop_requested != 0; },
    };
    const memoptima::sampling::SamplingSessionConfiguration configuration{
        .count = parsed_arguments->count,
        .interval = parsed_arguments->interval,
        .output_path = parsed_arguments->output_path,
    };

    const auto result = memoptima::sampling::run_sampling_session(configuration, dependencies);
    // Restore the process-wide SIGINT behavior after the bounded session exits.
    std::signal(SIGINT, previous_signal_handler);
    if (!result.has_value()) {
        std::cerr << "Error: Docker container sampling failed: " << result.error().detail << '\n';
        return kExitRuntimeError;
    }

    std::cerr << "Sampling session "
              << (result->outcome == memoptima::sampling::SamplingSessionOutcome::completed
                      ? "completed"
                  : result->outcome == memoptima::sampling::SamplingSessionOutcome::interrupted
                      ? "interrupted"
                      : "failed")
              << ": " << result->completed_sample_count
              << " sample(s), artifact: " << configuration.output_path.string() << '\n';
    return result->outcome == memoptima::sampling::SamplingSessionOutcome::failed
               ? kExitRuntimeError
               : kExitSuccess;
}

int run_processes_command() {
    const auto scan_result = memoptima::linux::scan_process_memory();
    if (!scan_result.has_value()) {
        std::cerr << "Error: unable to scan process memory information: "
                  << scan_result.error().detail << '\n';
        return kExitRuntimeError;
    }

    std::cout << memoptima::cli::format_process_scan_result(*scan_result);
    return kExitSuccess;
}

int run_containers_command() {
    const auto environment = memoptima::linux::detect_cgroup_v2_environment();
    if (!environment.has_value()) {
        std::cerr << "Error: Docker container observation is unavailable: "
                  << environment.error().detail << '\n';
        return kExitRuntimeError;
    }

    const memoptima::DockerContainerScanResult scan_result =
        memoptima::linux::scan_rootful_docker_containers(*environment);
    std::cout << memoptima::cli::format_docker_container_scan_result(scan_result);
    std::cerr << memoptima::cli::format_docker_container_scan_warnings(scan_result);
    return kExitSuccess;
}

int run_container_command(std::string_view container_id_text) {
    const auto container_id = memoptima::DockerContainerId::parse(container_id_text);
    if (!container_id.has_value()) {
        std::cerr << "Error: invalid Docker container ID: " << container_id.error().detail
                  << "\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }

    const auto environment = memoptima::linux::detect_cgroup_v2_environment();
    if (!environment.has_value()) {
        std::cerr << "Error: Docker container observation is unavailable: "
                  << environment.error().detail << '\n';
        return kExitRuntimeError;
    }

    const memoptima::DockerContainerScanResult scan_result =
        memoptima::linux::scan_rootful_docker_containers(*environment);
    std::cerr << memoptima::cli::format_docker_container_scan_warnings(scan_result);
    const auto container =
        std::ranges::find_if(scan_result.containers(),
                             [&container_id](const memoptima::DockerContainerMemorySnapshot &item) {
                                 return item.container_id().value() == container_id->value();
                             });
    if (container == scan_result.containers().end()) {
        std::cerr << "Error: Docker container '" << container_id->value()
                  << "' was not found in supported rootful Docker cgroup locations.\n";
        return kExitRuntimeError;
    }

    std::cout << memoptima::cli::format_docker_container_detail(*container);
    return kExitSuccess;
}

int run_assess_containers_command() {
    const auto environment = memoptima::linux::detect_cgroup_v2_environment();
    if (!environment.has_value()) {
        std::cerr << "Error: Docker container assessment is unavailable: "
                  << environment.error().detail << '\n';
        return kExitRuntimeError;
    }

    const memoptima::DockerContainerAssessmentScanResult assessment_result =
        memoptima::assess_docker_container_scan_result(
            memoptima::linux::scan_rootful_docker_containers(*environment));
    std::cout << memoptima::cli::format_docker_container_assessment_scan_result(assessment_result);
    std::cerr << memoptima::cli::format_docker_container_scan_warnings(
        assessment_result.warnings());
    return kExitSuccess;
}

int run_assess_container_command(std::string_view container_id_text) {
    const auto container_id = memoptima::DockerContainerId::parse(container_id_text);
    if (!container_id.has_value()) {
        std::cerr << "Error: invalid Docker container ID: " << container_id.error().detail
                  << "\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }

    const auto environment = memoptima::linux::detect_cgroup_v2_environment();
    if (!environment.has_value()) {
        std::cerr << "Error: Docker container assessment is unavailable: "
                  << environment.error().detail << '\n';
        return kExitRuntimeError;
    }

    const memoptima::DockerContainerAssessmentScanResult assessment_result =
        memoptima::assess_docker_container_scan_result(
            memoptima::linux::scan_rootful_docker_containers(*environment));
    std::cerr << memoptima::cli::format_docker_container_scan_warnings(
        assessment_result.warnings());
    const auto container =
        std::ranges::find_if(assessment_result.containers(),
                             [&container_id](const memoptima::DockerContainerAssessment &item) {
                                 return item.container_id().value() == container_id->value();
                             });
    if (container == assessment_result.containers().end()) {
        std::cerr << "Error: Docker container '" << container_id->value()
                  << "' was not found in supported rootful Docker cgroup locations.\n";
        return kExitRuntimeError;
    }

    std::cout << memoptima::cli::format_docker_container_assessment_detail(*container);
    return kExitSuccess;
}

int run(int argument_count, char *arguments[]) {
    if (argument_count == 1) {
        print_help(std::cout);
        return kExitSuccess;
    }

    if (argument_count == 3 && std::string_view{arguments[1]} == "container") {
        return run_container_command(arguments[2]);
    }

    if (argument_count == 3 && std::string_view{arguments[1]} == "assess" &&
        std::string_view{arguments[2]} == "containers") {
        return run_assess_containers_command();
    }

    if (argument_count == 4 && std::string_view{arguments[1]} == "assess" &&
        std::string_view{arguments[2]} == "container") {
        return run_assess_container_command(arguments[3]);
    }

    if (argument_count == 4 && std::string_view{arguments[1]} == "inspect" &&
        std::string_view{arguments[2]} == "evidence")
        return run_inspect_evidence_command(arguments[3]);

    if (argument_count >= 2 && std::string_view{arguments[1]} == "sample") {
        if (argument_count < 3 || std::string_view{arguments[2]} != "containers") {
            std::cerr << "Error: 'sample' requires 'containers' followed by sampling options.\n\n";
            print_help(std::cerr);
            return kExitUsageError;
        }
        return run_sample_containers_command(argument_count, arguments);
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

    if (option == "monitor") {
        return run_monitor_command();
    }

    if (option == "processes") {
        return run_processes_command();
    }

    if (option == "containers") {
        return run_containers_command();
    }

    if (option == "container") {
        std::cerr << "Error: 'container' requires a full Docker container ID.\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }

    if (option == "assess") {
        std::cerr << "Error: 'assess' requires 'containers' or 'container <FULL_ID>'.\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }

    if (option == "inspect") {
        std::cerr << "Error: 'inspect' requires 'evidence <FILE>'.\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }

    if (option == "sample") {
        std::cerr << "Error: 'sample' requires 'containers' followed by sampling options.\n\n";
        print_help(std::cerr);
        return kExitUsageError;
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

} // namespace

int main(int argument_count, char *arguments[]) { return run(argument_count, arguments); }
