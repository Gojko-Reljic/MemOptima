#include "cli/forecast_evaluation_formatter.hpp"

#include "cli/formatting.hpp"

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

constexpr std::size_t kColumnCount = 8U;
constexpr std::array<std::string_view, kColumnCount> kHeaders{
    "CONTAINER ID", "CGROUP PATH", "EPOCH", "HORIZON", "ORIGINS", "MAE", "BIAS", "STATUS"};
constexpr std::array<bool, kColumnCount> kRightAligned{false, false, true, true,
                                                       true,  true,  true, false};
constexpr std::array<std::string_view, 5U> kByteUnits{"B", "KiB", "MiB", "GiB", "TiB"};
constexpr double kBytesPerUnit = 1024.0;
using Row = std::array<std::string, kColumnCount>;

[[nodiscard]] std::string format_double_bytes(double bytes, bool signed_value = false) {
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
    return value ? format_double_bytes(*value, signed_value) : "N/A";
}

[[nodiscard]] std::string format_horizon(const std::optional<std::chrono::milliseconds> &horizon) {
    return horizon ? format_operational_duration(*horizon) : "N/A";
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
    case horizon_below_sampling_interval:
        return "horizon below sampling interval";
    case holdout_observation_unavailable:
        return "holdout observation unavailable";
    case insufficient_origins:
        return "insufficient origins";
    case numeric_result_unavailable:
        return "numeric result unavailable";
    }
    return "numeric result unavailable";
}

[[nodiscard]] Row format_row(const ForecastEvaluationRow &row) {
    return {escape_terminal_text(row.identity.container_id),
            escape_terminal_text(row.identity.cgroup_path),
            std::to_string(row.epoch_index),
            format_horizon(row.horizon),
            std::to_string(row.origin_count),
            format_optional_bytes(row.mean_absolute_error_bytes),
            format_optional_bytes(row.mean_signed_error_bytes, true),
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
    output << "Forecast evaluation\n\n"
           << "Evaluation rows:      " << summary.row_count << '\n'
           << "Evaluated epoch pairs:" << summary.evaluated_epoch_horizon_count << '\n';
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
    std::vector<Row> rows;
    rows.reserve(report.rows.size());
    for (const auto &row : report.rows)
        rows.push_back(format_row(row));
    std::array<std::size_t, kColumnCount> widths{};
    for (std::size_t index{}; index < kColumnCount; ++index)
        widths[index] = kHeaders[index].size();
    for (const auto &row : rows)
        for (std::size_t index{}; index < kColumnCount; ++index)
            widths[index] = std::max(widths[index], row[index].size());
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
