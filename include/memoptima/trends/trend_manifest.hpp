#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace memoptima::trends {

inline constexpr std::size_t kTrendManifestMaximumBytes = 1024U * 1024U;
inline constexpr std::size_t kTrendManifestMaximumArtifacts = 128U;

struct TrendManifest {
    std::filesystem::path directory;
    std::vector<std::filesystem::path> artifact_paths;
};

enum class TrendManifestParseErrorCode {
    invalid_json,
    nesting_too_deep,
    duplicate_key,
    root_not_object,
    missing_required_field,
    wrong_type,
    unknown_field,
    unsupported_manifest_version,
    invalid_artifact_count,
    invalid_artifact_path,
    duplicate_artifact_path,
};

struct TrendManifestParseError {
    TrendManifestParseErrorCode code;
    std::string json_path;
    std::string detail;
};

enum class TrendManifestLoadErrorCode {
    input_open_failed,
    input_is_symbolic_link,
    input_not_regular_file,
    input_read_failed,
    input_too_large,
    parse_failed,
};

struct TrendManifestLoadError {
    TrendManifestLoadErrorCode code;
    std::string detail;
};

[[nodiscard]] std::expected<TrendManifest, TrendManifestParseError>
parse_trend_manifest(std::string_view json, const std::filesystem::path &directory);

[[nodiscard]] std::expected<TrendManifest, TrendManifestLoadError>
load_trend_manifest(const std::filesystem::path &manifest_path);

} // namespace memoptima::trends
