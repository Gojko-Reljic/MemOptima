#include "application_runner.hpp"

#include <memoptima/analysis/memory_pressure.hpp>
#include <memoptima/analysis/memory_status.hpp>
#include <memoptima/application_info.hpp>
#include <memoptima/docker_container_assessment.hpp>
#include <memoptima/evidence_inspector.hpp>
#include <memoptima/forecast/evidence_forecast.hpp>
#include <memoptima/linux/cgroup_v2_capability_checker.hpp>
#include <memoptima/linux/docker_container_scanner.hpp>
#include <memoptima/linux/memory_reader.hpp>
#include <memoptima/linux/process_scanner.hpp>
#include <memoptima/sampling/sampling_configuration.hpp>
#include <memoptima/sampling/session_runner.hpp>
#include <memoptima/trends/trend_analysis_service.hpp>
#include <memoptima/trends/trend_manifest.hpp>

#include "assessments_formatter.hpp"
#include "containers_formatter.hpp"
#include "forecast_formatter.hpp"
#include "monitor_runner.hpp"
#include "processes_formatter.hpp"
#include "status_formatter.hpp"
#include "trends_formatter.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <expected>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace memoptima::cli {
namespace {

constexpr int kExitSuccess = 0;
constexpr int kExitRuntimeError = 1;
constexpr int kExitUsageError = 2;

using CommandArguments = std::span<const std::string_view>;
using CommandHandler = int (*)(CommandArguments);

struct Command {
    std::string_view name;
    CommandHandler handler;
};

// A signal handler only sets this signal-safe flag; normal code performs all cleanup.
volatile std::sig_atomic_t monitor_stop_requested = 0;
volatile std::sig_atomic_t sampling_stop_requested = 0;

void request_monitor_stop(int) noexcept { monitor_stop_requested = 1; }
void request_sampling_stop(int) noexcept { sampling_stop_requested = 1; }

void print_help(std::ostream &output) {
    output << application_name() << " — " << application_description() << "\n\n"
           << "Usage:\n"
           << "  memoptima [OPTION]\n"
           << "  memoptima container <FULL_ID>\n\n"
           << "  memoptima assess containers\n"
           << "  memoptima assess container <FULL_ID>\n"
           << "  memoptima inspect evidence <FILE>\n"
           << "  memoptima forecast evidence <FILE>\n"
           << "  memoptima trends analyze <MANIFEST>\n"
           << "  memoptima sample containers --count <COUNT> --interval <DURATION> --output "
              "<NEW_FILE>\n\n"
           << "Options:\n"
           << "  -h, --help       Display this help message.\n"
           << "  assess           Assess Docker container memory-boundary and OOM signals.\n"
           << "  containers       Display memory information for Docker containers.\n"
           << "  monitor          Continuously display memory status every five seconds.\n"
           << "  inspect          Validate and summarize a local evidence artifact.\n"
           << "  forecast         Estimate memory.max crossings from a local evidence artifact.\n"
           << "  processes        Display the top memory-consuming processes.\n"
           << "  sample           Collect bounded Docker cgroup memory evidence.\n"
           << "  status           Display current RAM and swap status.\n"
           << "  trends           Analyze cross-session container memory evidence.\n"
           << "  -v, --version    Display the application version.\n";
}

int report_expected_at_most_one_option() {
    std::cerr << "Error: expected at most one option.\n\n";
    print_help(std::cerr);
    return kExitUsageError;
}

[[nodiscard]] std::string format_memory_read_error_detail(const linux::MemoryReadError &error) {
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

[[nodiscard]] FormattedStatusResult read_formatted_memory_status() {
    const auto snapshot = linux::read_system_memory_snapshot();
    if (!snapshot.has_value()) {
        return std::unexpected(format_memory_read_error_detail(snapshot.error()));
    }
    const analysis::MemoryStatus status = analysis::evaluate_memory_status(*snapshot);
    const analysis::MemoryPressure pressure = analysis::evaluate_memory_pressure(status);
    return format_memory_status(status, pressure);
}

int run_status_command(CommandArguments arguments) {
    if (!arguments.empty()) {
        return report_expected_at_most_one_option();
    }
    const auto formatted_status = read_formatted_memory_status();
    if (!formatted_status.has_value()) {
        std::cerr << "Error: unable to read system memory information:\n"
                  << formatted_status.error() << '\n';
        return kExitRuntimeError;
    }
    std::cout << *formatted_status;
    return kExitSuccess;
}

int run_monitor_command(CommandArguments arguments) {
    if (!arguments.empty()) {
        return report_expected_at_most_one_option();
    }
    monitor_stop_requested = 0;
    const auto previous_signal_handler = std::signal(SIGINT, request_monitor_stop);
    if (previous_signal_handler == SIG_ERR) {
        std::cerr << "Error: unable to register the monitor interrupt handler.\n";
        return kExitRuntimeError;
    }
    const MonitorDependencies dependencies{
        .current_utc_time = [] { return std::chrono::system_clock::now(); },
        .current_steady_time = [] { return std::chrono::steady_clock::now(); },
        .read_formatted_status =
            [] {
                const auto formatted_status = read_formatted_memory_status();
                if (!formatted_status.has_value()) {
                    return FormattedStatusResult{std::unexpected(
                        "unable to read system memory information: " + formatted_status.error())};
                }
                return formatted_status;
            },
        .sleep_for =
            [](std::chrono::milliseconds duration) { std::this_thread::sleep_for(duration); },
        .stop_requested = [] { return monitor_stop_requested != 0; },
    };
    run_monitor(dependencies, std::cout, std::cerr);
    // Restore the process-wide SIGINT behavior after the monitor exits.
    std::signal(SIGINT, previous_signal_handler);
    return kExitSuccess;
}

int run_processes_command(CommandArguments arguments) {
    if (!arguments.empty()) {
        return report_expected_at_most_one_option();
    }
    const auto scan_result = linux::scan_process_memory();
    if (!scan_result.has_value()) {
        std::cerr << "Error: unable to scan process memory information: "
                  << scan_result.error().detail << '\n';
        return kExitRuntimeError;
    }
    std::cout << format_process_scan_result(*scan_result);
    return kExitSuccess;
}

int run_containers_command(CommandArguments arguments) {
    if (!arguments.empty()) {
        return report_expected_at_most_one_option();
    }
    const auto environment = linux::detect_cgroup_v2_environment();
    if (!environment.has_value()) {
        std::cerr << "Error: Docker container observation is unavailable: "
                  << environment.error().detail << '\n';
        return kExitRuntimeError;
    }
    const DockerContainerScanResult scan_result =
        linux::scan_rootful_docker_containers(*environment);
    std::cout << format_docker_container_scan_result(scan_result);
    std::cerr << format_docker_container_scan_warnings(scan_result);
    return kExitSuccess;
}

int run_container_command(CommandArguments arguments) {
    if (arguments.empty()) {
        std::cerr << "Error: 'container' requires a full Docker container ID.\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }
    if (arguments.size() != 1) {
        return report_expected_at_most_one_option();
    }
    const auto container_id = DockerContainerId::parse(arguments.front());
    if (!container_id.has_value()) {
        std::cerr << "Error: invalid Docker container ID: " << container_id.error().detail
                  << "\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }
    const auto environment = linux::detect_cgroup_v2_environment();
    if (!environment.has_value()) {
        std::cerr << "Error: Docker container observation is unavailable: "
                  << environment.error().detail << '\n';
        return kExitRuntimeError;
    }
    const DockerContainerScanResult scan_result =
        linux::scan_rootful_docker_containers(*environment);
    std::cerr << format_docker_container_scan_warnings(scan_result);
    const auto container = std::ranges::find_if(
        scan_result.containers, [&container_id](const DockerContainerMemorySnapshot &item) {
            return item.container_id.value() == container_id->value();
        });
    if (container == scan_result.containers.end()) {
        std::cerr << "Error: Docker container '" << container_id->value()
                  << "' was not found in supported rootful Docker cgroup locations.\n";
        return kExitRuntimeError;
    }
    std::cout << format_docker_container_detail(*container);
    return kExitSuccess;
}

int run_assess_containers_command() {
    const auto environment = linux::detect_cgroup_v2_environment();
    if (!environment.has_value()) {
        std::cerr << "Error: Docker container assessment is unavailable: "
                  << environment.error().detail << '\n';
        return kExitRuntimeError;
    }
    const DockerContainerAssessmentScanResult assessment_result =
        assess_docker_container_scan_result(linux::scan_rootful_docker_containers(*environment));
    std::cout << format_docker_container_assessment_scan_result(assessment_result);
    std::cerr << format_docker_container_scan_warnings(assessment_result.warnings);
    return kExitSuccess;
}

int run_assess_container_command(std::string_view container_id_text) {
    const auto container_id = DockerContainerId::parse(container_id_text);
    if (!container_id.has_value()) {
        std::cerr << "Error: invalid Docker container ID: " << container_id.error().detail
                  << "\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }
    const auto environment = linux::detect_cgroup_v2_environment();
    if (!environment.has_value()) {
        std::cerr << "Error: Docker container assessment is unavailable: "
                  << environment.error().detail << '\n';
        return kExitRuntimeError;
    }
    const DockerContainerAssessmentScanResult assessment_result =
        assess_docker_container_scan_result(linux::scan_rootful_docker_containers(*environment));
    std::cerr << format_docker_container_scan_warnings(assessment_result.warnings);
    const auto container = std::ranges::find_if(
        assessment_result.containers, [&container_id](const DockerContainerAssessment &item) {
            return item.container_id.value() == container_id->value();
        });
    if (container == assessment_result.containers.end()) {
        std::cerr << "Error: Docker container '" << container_id->value()
                  << "' was not found in supported rootful Docker cgroup locations.\n";
        return kExitRuntimeError;
    }
    std::cout << format_docker_container_assessment_detail(*container);
    return kExitSuccess;
}

int run_assess_command(CommandArguments arguments) {
    if (arguments.empty()) {
        std::cerr << "Error: 'assess' requires 'containers' or 'container <FULL_ID>'.\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }
    if (arguments.front() == "containers" && arguments.size() == 1) {
        return run_assess_containers_command();
    }
    if (arguments.front() == "container" && arguments.size() == 2) {
        return run_assess_container_command(arguments[1]);
    }
    return report_expected_at_most_one_option();
}

int run_inspect_evidence_command(std::string_view path_text) {
    const auto summary = inspect_evidence_artifact(std::filesystem::path{path_text});
    if (!summary.has_value()) {
        std::cerr << "Error: unable to inspect evidence artifact: " << summary.error().detail
                  << '\n';
        return kExitRuntimeError;
    }
    std::cout << "Evidence artifact\n\n"
              << "Path: " << summary->artifact_path.string() << '\n'
              << "Schema version: " << summary->schema_version << '\n'
              << "Application version: " << summary->application_version << '\n'
              << "Requested samples: " << summary->requested_sample_count << '\n'
              << "Requested interval: " << summary->requested_interval_ms << " ms\n"
              << "Completed samples: " << summary->completed_sample_count << '\n'
              << "Completion: " << (summary->incomplete ? "Incomplete" : summary->terminal_outcome)
              << '\n'
              << "Discovery warnings: " << summary->discovery_warning_count << '\n'
              << "Skipped containers: " << summary->skipped_container_count << "\n\n"
              << "First capture: "
              << (summary->first_capture_at_utc.empty() ? "None" : summary->first_capture_at_utc)
              << '\n'
              << "Last capture: "
              << (summary->last_capture_at_utc.empty() ? "None" : summary->last_capture_at_utc)
              << "\n\n"
              << "Containers\n";
    if (summary->containers.empty()) {
        std::cout << "  None\n";
    }
    for (const auto &container : summary->containers) {
        std::cout << "  " << container.container_id << "  samples=" << container.samples_seen
                  << " highest_risk=" << container.highest_risk
                  << " partial_samples=" << container.partial_coverage_samples
                  << " last=" << container.last_risk << '/' << container.last_coverage << '\n';
    }
    if (summary->incomplete) {
        std::cout << "\nWarning: Incomplete: terminal session_end record is missing.\n";
    }
    return kExitSuccess;
}

int run_inspect_command(CommandArguments arguments) {
    if (arguments.empty()) {
        std::cerr << "Error: 'inspect' requires 'evidence <FILE>'.\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }
    if (arguments.front() == "evidence" && arguments.size() == 2) {
        return run_inspect_evidence_command(arguments[1]);
    }
    return report_expected_at_most_one_option();
}

int run_forecast_command(CommandArguments arguments) {
    if (arguments.size() != 2 || arguments.front() != "evidence") {
        std::cerr << "Error: 'forecast' requires 'evidence <FILE>'.\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }
    const auto summary = forecast::forecast_evidence_artifact(std::filesystem::path{arguments[1]});
    if (!summary.has_value()) {
        std::cerr << "Error: unable to forecast evidence artifact: " << summary.error().detail
                  << '\n';
        return kExitRuntimeError;
    }
    std::cout << format_evidence_forecast(*summary);
    return kExitSuccess;
}

int run_trends_command(CommandArguments arguments) {
    if (arguments.size() != 2U || arguments.front() != "analyze") {
        std::cerr << "Error: 'trends' requires 'analyze <MANIFEST>'.\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }

    const auto manifest = trends::load_trend_manifest(std::filesystem::path{arguments[1]});
    if (!manifest.has_value()) {
        std::cerr << "Error: unable to load trend manifest: " << manifest.error().detail << '\n';
        return kExitRuntimeError;
    }

    const auto analysis = trends::analyze_trend_manifest(*manifest);
    if (!analysis.has_value()) {
        std::cerr << "Error: unable to analyze trend manifest: " << analysis.error().detail << '\n';
        return kExitRuntimeError;
    }

    std::cout << format_trend_analysis(*analysis);
    if (!std::cout) {
        std::cerr << "Error: unable to write trend analysis table to standard output.\n";
        return kExitRuntimeError;
    }
    return kExitSuccess;
}

struct SampleContainersArguments {
    sampling::SamplingCount count;
    sampling::SamplingInterval interval;
    std::filesystem::path output_path;
};

[[nodiscard]] std::expected<SampleContainersArguments, std::string>
parse_sample_containers_arguments(CommandArguments arguments) {
    if (arguments.size() != 7) {
        return std::unexpected(
            "'sample containers' requires --count, --interval, and --output exactly once.");
    }
    std::optional<std::string_view> count_text;
    std::optional<std::string_view> interval_text;
    std::optional<std::string_view> output_text;
    for (std::size_t index = 1; index < arguments.size(); index += 2) {
        const std::string_view option = arguments[index];
        const std::string_view value = arguments[index + 1];
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
    const auto count = sampling::parse_sampling_count(*count_text);
    if (!count.has_value()) {
        return std::unexpected("invalid --count: " + count.error().detail);
    }
    const auto interval = sampling::parse_sampling_interval(*interval_text);
    if (!interval.has_value()) {
        return std::unexpected("invalid --interval: " + interval.error().detail);
    }
    return SampleContainersArguments{
        .count = *count,
        .interval = *interval,
        .output_path = std::filesystem::path{*output_text},
    };
}

int run_sample_containers_command(CommandArguments arguments) {
    const auto parsed_arguments = parse_sample_containers_arguments(arguments);
    if (!parsed_arguments.has_value()) {
        std::cerr << "Error: " << parsed_arguments.error() << "\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }
    std::optional<linux::CgroupV2Environment> environment;
    sampling_stop_requested = 0;
    const auto previous_signal_handler = std::signal(SIGINT, request_sampling_stop);
    if (previous_signal_handler == SIG_ERR) {
        std::cerr << "Error: unable to register the sampling interrupt handler.\n";
        return kExitRuntimeError;
    }
    const sampling::SamplingSessionDependencies dependencies{
        .preflight = [&environment] -> std::expected<void, std::string> {
            const auto detected_environment = linux::detect_cgroup_v2_environment();
            if (!detected_environment.has_value()) {
                return std::unexpected(detected_environment.error().detail);
            }
            environment = *detected_environment;
            return {};
        },
        .scan_containers =
            [&environment] { return linux::scan_rootful_docker_containers(*environment); },
        .current_utc_time = [] { return std::chrono::system_clock::now(); },
        .current_steady_time = [] { return std::chrono::steady_clock::now(); },
        .sleep_for =
            [](std::chrono::milliseconds duration) { std::this_thread::sleep_for(duration); },
        .stop_requested = [] { return sampling_stop_requested != 0; },
    };
    const sampling::SamplingSessionConfiguration configuration{
        .count = parsed_arguments->count,
        .interval = parsed_arguments->interval,
        .output_path = parsed_arguments->output_path,
    };
    const auto result = sampling::run_sampling_session(configuration, dependencies);
    // Restore the process-wide SIGINT behavior after the bounded session exits.
    std::signal(SIGINT, previous_signal_handler);
    if (!result.has_value()) {
        std::cerr << "Error: Docker container sampling failed: " << result.error().detail << '\n';
        return kExitRuntimeError;
    }
    std::cerr << "Sampling session "
              << (result->outcome == sampling::SamplingSessionOutcome::completed     ? "completed"
                  : result->outcome == sampling::SamplingSessionOutcome::interrupted ? "interrupted"
                                                                                     : "failed")
              << ": " << result->completed_sample_count
              << " sample(s), artifact: " << configuration.output_path.string() << '\n';
    return result->outcome == sampling::SamplingSessionOutcome::failed ? kExitRuntimeError
                                                                       : kExitSuccess;
}

int run_sample_command(CommandArguments arguments) {
    if (arguments.empty() || arguments.front() != "containers") {
        std::cerr << "Error: 'sample' requires 'containers' followed by sampling options.\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }
    return run_sample_containers_command(arguments);
}

int run_help_command(CommandArguments arguments) {
    if (!arguments.empty()) {
        return report_expected_at_most_one_option();
    }
    print_help(std::cout);
    return kExitSuccess;
}

int run_version_command(CommandArguments arguments) {
    if (!arguments.empty()) {
        return report_expected_at_most_one_option();
    }
    std::cout << "memoptima " << version() << '\n';
    return kExitSuccess;
}

constexpr std::array kCommands{
    Command{"status", run_status_command},
    Command{"monitor", run_monitor_command},
    Command{"processes", run_processes_command},
    Command{"containers", run_containers_command},
    Command{"container", run_container_command},
    Command{"assess", run_assess_command},
    Command{"inspect", run_inspect_command},
    Command{"forecast", run_forecast_command},
    Command{"trends", run_trends_command},
    Command{"sample", run_sample_command},
    Command{"-h", run_help_command},
    Command{"--help", run_help_command},
    Command{"-v", run_version_command},
    Command{"--version", run_version_command},
};

} // namespace

int run_application(int argument_count, char *argv[]) {
    std::vector<std::string_view> command_arguments;
    command_arguments.reserve(static_cast<std::size_t>(std::max(argument_count - 1, 0)));
    for (int index = 1; index < argument_count; ++index) {
        command_arguments.emplace_back(argv[index]);
    }
    const CommandArguments parsed_arguments{command_arguments};
    if (parsed_arguments.empty()) {
        return run_help_command({});
    }
    if (parsed_arguments.size() > 1 &&
        std::ranges::find(kCommands, parsed_arguments.front(), &Command::name) == kCommands.end()) {
        return report_expected_at_most_one_option();
    }
    const auto command = std::ranges::find(kCommands, parsed_arguments.front(), &Command::name);
    if (command == kCommands.end()) {
        std::cerr << "Error: unknown option '" << parsed_arguments.front() << "'.\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }
    return command->handler(parsed_arguments.subspan(1));
}

} // namespace memoptima::cli
