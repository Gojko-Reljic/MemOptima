#include "cli/forecast_evaluation_formatter.hpp"

#include "cli/formatting.hpp"

#include <memoptima/utc_timestamp.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <iomanip>
#include <map>
#include <sstream>
#include <string_view>
#include <vector>

namespace memoptima::cli {
namespace {

using forecast_evaluation::ForecastEvaluationRow;
using forecast_evaluation::ForecastEvaluationStatus;

constexpr std::size_t kColumnCount = 11U;
constexpr std::array<std::string_view, kColumnCount> kHeaders{
    "CONTAINER ID", "CGROUP PATH", "EPOCH",  "HISTORY", "WINDOW", "FORECAST AT",
    "HOLDOUT AT",   "PREDICTED",   "ACTUAL", "ERROR",   "STATUS",
};
constexpr std::array<bool, kColumnCount> kRightAligned{false, false, true, true, true, false,
                                                       false, true,  true, true, false};
constexpr std::array<std::string_view, 5U> kByteUnits{"B", "KiB", "MiB", "GiB", "TiB"};
constexpr double kBytesPerUnit = 1024.0;

using Row = std::array<std::string, kColumnCount>;

[[nodiscard]] std::size_t common_prefix_length(std::string_view left, std::string_view right) {
    const auto maximum = std::min(left.size(), right.size());
    std::size_t length{};
    while (length < maximum && left[length] == right[length])
        ++length;
    return length;
}

[[nodiscard]] std::vector<std::size_t>
container_identifier_widths(const std::vector<ForecastEvaluationRow> &evaluation_rows) {
    std::vector<std::string_view> identifiers;
    identifiers.reserve(evaluation_rows.size());
    for (const auto &row : evaluation_rows) {
        identifiers.push_back(row.identity.container_id);
    }
    std::ranges::sort(identifiers);
    const auto unique_end = std::ranges::unique(identifiers).begin();
    identifiers.erase(unique_end, identifiers.end());

    std::map<std::string_view, std::size_t> widths_by_identifier;
    for (std::size_t index{}; index < identifiers.size(); ++index) {
        const auto identifier = identifiers[index];
        std::size_t width = std::min<std::size_t>(12U, identifier.size());
        if (index > 0U)
            width = std::max(width, common_prefix_length(identifier, identifiers[index - 1U]) + 1U);
        if (index + 1U < identifiers.size())
            width = std::max(width, common_prefix_length(identifier, identifiers[index + 1U]) + 1U);
        widths_by_identifier.emplace(identifier, std::min(width, identifier.size()));
    }

    std::vector<std::size_t> widths;
    widths.reserve(evaluation_rows.size());
    for (const auto &row : evaluation_rows)
        widths.push_back(widths_by_identifier.at(row.identity.container_id));
    return widths;
}

[[nodiscard]] std::string format_double_bytes(double bytes, bool signed_value) {
    double display_value = bytes;
    std::size_t unit_index{};
    while (std::abs(display_value) >= kBytesPerUnit && unit_index + 1U < kByteUnits.size()) {
        display_value /= kBytesPerUnit;
        ++unit_index;
    }
    if (std::abs(display_value) < 0.005)
        display_value = 0.0;
    return signed_value ? std::format("{:+.2f} {}", display_value, kByteUnits[unit_index])
                        : std::format("{:.2f} {}", display_value, kByteUnits[unit_index]);
}

[[nodiscard]] std::string format_optional_bytes(const std::optional<double> &value,
                                                bool signed_value = false) {
    return value.has_value() ? format_double_bytes(*value, signed_value) : "N/A";
}

[[nodiscard]] std::string format_optional_actual(const std::optional<std::uint64_t> &value) {
    return value.has_value() ? format_bytes(*value) : "N/A";
}

[[nodiscard]] std::string
format_timestamp(const std::optional<forecast_evaluation::ForecastEvaluationTimePoint> &value) {
    if (!value.has_value())
        return "N/A";
    const auto formatted =
        format_utc_timestamp(std::chrono::system_clock::time_point{value->time_since_epoch()},
                             UtcTimestampPrecision::milliseconds);
    return formatted.has_value() ? *formatted : "N/A";
}

[[nodiscard]] std::string format_window(std::uint64_t milliseconds) {
    constexpr auto maximum =
        static_cast<std::uint64_t>(std::numeric_limits<std::chrono::milliseconds::rep>::max());
    if (milliseconds > maximum)
        return "N/A";
    return format_operational_duration(
        std::chrono::milliseconds{static_cast<std::chrono::milliseconds::rep>(milliseconds)});
}

[[nodiscard]] std::string status_name(ForecastEvaluationStatus status) {
    using enum ForecastEvaluationStatus;
    switch (status) {
    case evaluated:
        return "evaluated";
    case excluded_incomplete_session:
        return "excluded incomplete session";
    case insufficient_observations:
        return "insufficient observations";
    case insufficient_observation_window:
        return "insufficient observation window";
    case container_not_observed_continuously:
        return "container not observed continuously";
    case historical_current_memory_unavailable:
        return "historical memory.current unavailable";
    case non_increasing_timestamps:
        return "non-increasing timestamps";
    case holdout_observation_unavailable:
        return "holdout observation unavailable";
    case numeric_result_unavailable:
        return "numeric result unavailable";
    }
    return "numeric result unavailable";
}

[[nodiscard]] Row format_row(const ForecastEvaluationRow &row, std::size_t identifier_width) {
    return {row.identity.container_id.substr(0U, identifier_width),
            escape_terminal_cell(row.identity.cgroup_path),
            std::to_string(row.epoch_index),
            std::to_string(row.history_observation_count),
            format_window(row.history_window_ms),
            format_timestamp(row.history_last_timestamp),
            format_timestamp(row.holdout_timestamp),
            format_optional_bytes(row.predicted_current_bytes),
            format_optional_actual(row.actual_current_bytes),
            format_optional_bytes(row.signed_error_bytes, true),
            status_name(row.status)};
}

void append_row(std::ostringstream &output, const Row &row,
                const std::array<std::size_t, kColumnCount> &widths) {
    for (std::size_t index{}; index < kColumnCount; ++index) {
        if (index + 1U == kColumnCount) {
            output << row[index] << '\n';
            return;
        }
        output << (kRightAligned[index] ? std::right : std::left)
               << std::setw(static_cast<int>(widths[index])) << row[index] << "  ";
    }
}

void append_summary(std::ostringstream &output,
                    const forecast_evaluation::ForecastEvaluationSummary &summary) {
    const auto coverage = summary.coverage.has_value()
                              ? format_percentage(*summary.coverage * 100.0, 2U)
                              : std::string{"N/A"};
    const auto mae = format_optional_bytes(summary.mean_absolute_error_bytes);
    const auto bias = format_optional_bytes(summary.mean_signed_error_bytes, true);
    output << "Forecast evaluation\n\n"
           << "Eligible forecasts:   " << summary.eligible_forecast_count << '\n'
           << "Evaluated forecasts:  " << summary.evaluated_forecast_count << '\n'
           << "Coverage:             " << coverage << '\n'
           << "MAE:                  " << mae << '\n'
           << "Bias:                 " << bias << '\n'
           << "Excluded incomplete:  " << summary.excluded_incomplete_session_count << '\n';
    if (!summary.status_counts.empty()) {
        output << "\nStatus counts:\n";
        for (const auto &[status, count] : summary.status_counts)
            output << "  " << status_name(status) << ": " << count << '\n';
    }
    output << '\n';
}

} // namespace

std::string
format_forecast_evaluation(const forecast_evaluation::ForecastEvaluationReport &report) {
    if (report.rows.empty())
        return "No forecast evaluation epochs found in manifest.\n";

    const auto identifier_widths = container_identifier_widths(report.rows);
    std::vector<Row> rows;
    rows.reserve(report.rows.size());
    for (std::size_t index{}; index < report.rows.size(); ++index)
        rows.push_back(format_row(report.rows[index], identifier_widths[index]));

    std::array<std::size_t, kColumnCount> widths{};
    for (std::size_t index{}; index < kColumnCount; ++index)
        widths[index] = kHeaders[index].size();
    for (const auto &row : rows) {
        for (std::size_t index{}; index < kColumnCount; ++index)
            widths[index] = std::max(widths[index], row[index].size());
    }

    std::ostringstream output;
    append_summary(output, report.summary);
    Row header{};
    for (std::size_t index{}; index < kColumnCount; ++index)
        header[index] = kHeaders[index];
    append_row(output, header, widths);
    for (const auto &row : rows)
        append_row(output, row, widths);
    return std::move(output).str();
}

} // namespace memoptima::cli
