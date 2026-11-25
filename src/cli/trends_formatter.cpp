#include "cli/trends_formatter.hpp"

#include "cli/formatting.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <vector>

namespace memoptima::cli {
namespace {

constexpr std::size_t kColumnCount = 9U;
constexpr std::array<std::string_view, kColumnCount> kHeaders{
    "CONTAINER ID", "CGROUP PATH", "EPOCH",         "SAMPLES", "WINDOW",
    "QUALITY",      "RATE",        "TIME TO LIMIT", "STATUS"};
constexpr std::array<bool, kColumnCount> kRightAligned{false, false, true, true, true,
                                                       false, true,  true, false};
constexpr std::array<std::string_view, 5U> kRateUnits{"B", "KiB", "MiB", "GiB", "TiB"};
constexpr double kBytesPerUnit = 1024.0;

using Row = std::array<std::string, kColumnCount>;

[[nodiscard]] std::size_t common_prefix_length(std::string_view left, std::string_view right) {
    const auto maximum_length = std::min(left.size(), right.size());
    std::size_t length{};
    while (length < maximum_length && left[length] == right[length])
        ++length;
    return length;
}

[[nodiscard]] std::vector<std::size_t>
container_identifier_widths(const std::vector<trends::TrendEpochResult> &epochs) {
    std::vector<std::size_t> widths;
    widths.reserve(epochs.size());

    for (const auto &epoch : epochs) {
        const auto &identifier = epoch.identity.container_id;
        std::size_t width = std::min<std::size_t>(12U, identifier.size());

        // One character beyond every shared prefix keeps each displayed ID unambiguous.
        for (const auto &other_epoch : epochs) {
            const auto &other_identifier = other_epoch.identity.container_id;
            if (identifier == other_identifier)
                continue;

            width = std::max(width, common_prefix_length(identifier, other_identifier) + 1U);
        }
        widths.push_back(std::min(width, identifier.size()));
    }
    return widths;
}

[[nodiscard]] std::string format_rate(const trends::TrendCurrentMemoryResult &current_memory) {
    if (!current_memory.growth_bytes_per_second.has_value())
        return "N/A";

    double display_value = *current_memory.growth_bytes_per_second;
    std::size_t unit_index{};
    while (std::abs(display_value) >= kBytesPerUnit && unit_index + 1U < kRateUnits.size()) {
        display_value /= kBytesPerUnit;
        ++unit_index;
    }

    // A value that prints as zero must not retain the IEEE-754 negative-zero sign.
    if (std::abs(display_value) < 0.005)
        display_value = 0.0;

    return std::format("{:+.2f} {}/s", display_value, kRateUnits[unit_index]);
}

[[nodiscard]] std::string format_quality(const trends::TrendEpochResult &epoch) {
    const auto base =
        epoch.quality == trends::TrendEvidenceQuality::continuous ? "continuous" : "sparse";
    return epoch.contains_incomplete_session ? std::format("{} (incomplete)", base)
                                             : std::string{base};
}

[[nodiscard]] std::string format_status(const trends::TrendEpochResult &epoch) {
    using enum trends::TrendTimeToLimitStatus;

    switch (epoch.time_to_limit.status) {
    case available:
        return "available";
    case rate_unavailable:
        switch (epoch.current_memory.rate_status) {
        case trends::TrendRateStatus::insufficient_observations:
            return "insufficient observations";
        case trends::TrendRateStatus::insufficient_duration:
            return "insufficient observation window";
        case trends::TrendRateStatus::available:
            return "insufficient observations";
        }
        break;
    case non_positive_growth:
        return "non-positive growth";
    case sparse_evidence:
        return "sparse evidence";
    case unlimited_maximum:
        return "unlimited memory.max";
    case unavailable_maximum:
        return "unavailable memory.max";
    case already_at_or_above_maximum:
        return "at or above memory.max";
    case duration_out_of_range:
        return "estimate out of range";
    }

    return "insufficient observations";
}

[[nodiscard]] std::string format_duration_milliseconds(std::uint64_t milliseconds) {
    constexpr auto kMaximumDuration =
        static_cast<std::uint64_t>(std::numeric_limits<std::chrono::milliseconds::rep>::max());
    if (milliseconds > kMaximumDuration)
        return "N/A";

    return format_operational_duration(
        std::chrono::milliseconds{static_cast<std::chrono::milliseconds::rep>(milliseconds)});
}

[[nodiscard]] std::string
format_time_to_limit(const trends::TrendTimeToLimitResult &time_to_limit) {
    return time_to_limit.estimated_duration_ms.has_value()
               ? format_duration_milliseconds(*time_to_limit.estimated_duration_ms)
               : "N/A";
}

void append_row(std::ostringstream &output, const Row &row,
                const std::array<std::size_t, kColumnCount> &widths) {
    for (std::size_t index{}; index < kColumnCount; ++index) {
        if (index + 1U == kColumnCount) {
            // The final column has no right padding, preserving the no-trailing-whitespace
            // contract.
            output << row[index] << '\n';
            return;
        }

        output << (kRightAligned[index] ? std::right : std::left)
               << std::setw(static_cast<int>(widths[index])) << row[index];
        output << "  ";
    }
}

} // namespace

std::string format_trend_analysis(const trends::TrendAnalysis &analysis) {
    if (analysis.epochs.empty())
        return "No container observation epochs found in manifest.\n";

    const auto identifier_widths = container_identifier_widths(analysis.epochs);
    std::vector<Row> rows;
    rows.reserve(analysis.epochs.size());

    for (std::size_t index{}; index < analysis.epochs.size(); ++index) {
        const auto &epoch = analysis.epochs[index];
        rows.push_back({epoch.identity.container_id.substr(0U, identifier_widths[index]),
                        escape_terminal_cell(epoch.identity.cgroup_path),
                        std::to_string(epoch.epoch_index), std::to_string(epoch.observation_count),
                        format_duration_milliseconds(epoch.observation_window_ms),
                        format_quality(epoch), format_rate(epoch.current_memory),
                        format_time_to_limit(epoch.time_to_limit), format_status(epoch)});
    }

    std::array<std::size_t, kColumnCount> widths{};
    for (std::size_t index{}; index < kColumnCount; ++index)
        widths[index] = kHeaders[index].size();
    for (const auto &row : rows) {
        for (std::size_t index{}; index < kColumnCount; ++index)
            widths[index] = std::max(widths[index], row[index].size());
    }

    std::ostringstream output;
    Row header{};
    for (std::size_t index{}; index < kColumnCount; ++index)
        header[index] = kHeaders[index];
    append_row(output, header, widths);
    for (const auto &row : rows)
        append_row(output, row, widths);
    return std::move(output).str();
}

} // namespace memoptima::cli
