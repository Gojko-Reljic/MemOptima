#include <memoptima/trends/trend_manifest.hpp>

#include <memoptima/evidence_input_reader.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <set>
#include <string>
#include <system_error>
#include <utility>

namespace memoptima::trends {
namespace {

using Json = nlohmann::ordered_json;

[[nodiscard]] std::unexpected<TrendManifestParseError>
parse_error(TrendManifestParseErrorCode code, std::string json_path, std::string detail) {
    return std::unexpected(TrendManifestParseError{
        .code = code,
        .json_path = std::move(json_path),
        .detail = std::move(detail),
    });
}

[[nodiscard]] std::unexpected<TrendManifestLoadError> load_error(TrendManifestLoadErrorCode code,
                                                                 std::string detail) {
    return std::unexpected(TrendManifestLoadError{.code = code, .detail = std::move(detail)});
}

[[nodiscard]] bool has_exact_keys(const Json &object,
                                  std::initializer_list<std::string_view> keys) {
    if (object.size() != keys.size()) {
        return false;
    }
    return std::ranges::all_of(keys,
                               [&object](std::string_view key) { return object.contains(key); });
}

[[nodiscard]] std::expected<std::filesystem::path, TrendManifestParseError>
parse_artifact_path(std::string_view text, const std::filesystem::path &directory,
                    std::size_t index) {
    const std::string json_path = "$.artifacts[" + std::to_string(index) + "].path";
    if (text.empty()) {
        return parse_error(TrendManifestParseErrorCode::invalid_artifact_path, json_path,
                           "The artifact path must not be empty.");
    }
    if (text.find('\0') != std::string_view::npos) {
        return parse_error(TrendManifestParseErrorCode::invalid_artifact_path, json_path,
                           "The artifact path must not contain a NUL byte.");
    }

    const std::filesystem::path raw{text};
    if (raw.is_absolute()) {
        return parse_error(TrendManifestParseErrorCode::invalid_artifact_path, json_path,
                           "The artifact path must be relative.");
    }
    for (const auto &component : raw) {
        if (component == "..") {
            return parse_error(TrendManifestParseErrorCode::invalid_artifact_path, json_path,
                               "The artifact path must not escape the manifest directory.");
        }
    }

    const std::filesystem::path normalized = raw.lexically_normal();
    if (normalized.empty() || normalized == ".") {
        return parse_error(TrendManifestParseErrorCode::invalid_artifact_path, json_path,
                           "The artifact path must name a file.");
    }
    return (directory / normalized).lexically_normal();
}

[[nodiscard]] TrendManifestLoadErrorCode load_error_code(EvidenceInputErrorCode code) {
    switch (code) {
    case EvidenceInputErrorCode::input_open_failed:
        return TrendManifestLoadErrorCode::input_open_failed;
    case EvidenceInputErrorCode::input_is_symbolic_link:
        return TrendManifestLoadErrorCode::input_is_symbolic_link;
    case EvidenceInputErrorCode::input_not_regular_file:
        return TrendManifestLoadErrorCode::input_not_regular_file;
    case EvidenceInputErrorCode::input_read_failed:
        return TrendManifestLoadErrorCode::input_read_failed;
    case EvidenceInputErrorCode::line_too_long:
    case EvidenceInputErrorCode::input_too_large:
        return TrendManifestLoadErrorCode::input_too_large;
    }
    return TrendManifestLoadErrorCode::input_read_failed;
}

[[nodiscard]] bool is_within_directory(const std::filesystem::path &path,
                                       const std::filesystem::path &directory) {
    const std::filesystem::path relative = path.lexically_relative(directory);
    return !relative.empty() && *relative.begin() != "..";
}

[[nodiscard]] std::expected<std::filesystem::path, TrendManifestLoadError>
canonical_manifest_directory(const std::filesystem::path &manifest_path) {
    std::error_code error;
    const std::filesystem::path directory = std::filesystem::weakly_canonical(
        manifest_path.parent_path().empty() ? std::filesystem::path{"."}
                                            : manifest_path.parent_path(),
        error);
    if (error) {
        return load_error(TrendManifestLoadErrorCode::input_open_failed,
                          "Unable to resolve the manifest directory: " + error.message());
    }
    return directory;
}

[[nodiscard]] std::expected<void, TrendManifestLoadError>
validate_artifact_containment(TrendManifest &manifest) {
    for (std::size_t index{}; index < manifest.artifact_paths.size(); ++index) {
        std::error_code error;
        const std::filesystem::path resolved =
            std::filesystem::weakly_canonical(manifest.artifact_paths[index], error);
        if (error) {
            return load_error(
                TrendManifestLoadErrorCode::parse_failed,
                "$.artifacts[" + std::to_string(index) +
                    "].path: Unable to resolve the artifact path: " + error.message());
        }
        if (!is_within_directory(resolved, manifest.directory)) {
            return load_error(TrendManifestLoadErrorCode::parse_failed,
                              "$.artifacts[" + std::to_string(index) +
                                  "].path: The artifact path resolves outside the manifest "
                                  "directory.");
        }
    }
    return {};
}

} // namespace

std::expected<TrendManifest, TrendManifestParseError>
parse_trend_manifest(std::string_view text, const std::filesystem::path &directory) {
    const auto parsed = Json::parse(text, nullptr, false);
    if (parsed.is_discarded()) {
        return parse_error(TrendManifestParseErrorCode::invalid_json, "$",
                           "The manifest is not valid JSON.");
    }
    if (!parsed.is_object()) {
        return parse_error(TrendManifestParseErrorCode::root_not_object, "$",
                           "The manifest root must be an object.");
    }
    if (!parsed.contains("manifest_version") || !parsed.contains("artifacts")) {
        return parse_error(TrendManifestParseErrorCode::missing_required_field, "$",
                           "The manifest requires manifest_version and artifacts.");
    }
    if (!has_exact_keys(parsed, {"manifest_version", "artifacts"})) {
        return parse_error(TrendManifestParseErrorCode::unknown_field, "$",
                           "The manifest contains an unknown field.");
    }
    if (!parsed.at("manifest_version").is_number_unsigned()) {
        return parse_error(TrendManifestParseErrorCode::wrong_type, "$.manifest_version",
                           "The manifest version must be an unsigned integer.");
    }
    if (parsed.at("manifest_version") != 1U) {
        return parse_error(TrendManifestParseErrorCode::unsupported_manifest_version,
                           "$.manifest_version", "Only manifest_version 1 is supported.");
    }
    if (!parsed.at("artifacts").is_array()) {
        return parse_error(TrendManifestParseErrorCode::wrong_type, "$.artifacts",
                           "The artifacts field must be an array.");
    }

    const auto &artifacts = parsed.at("artifacts");
    if (artifacts.empty() || artifacts.size() > kTrendManifestMaximumArtifacts) {
        return parse_error(TrendManifestParseErrorCode::invalid_artifact_count, "$.artifacts",
                           "The manifest must contain from 1 through 128 artifacts.");
    }

    TrendManifest manifest{
        .directory = directory.lexically_normal(),
        .artifact_paths = {},
    };
    std::set<std::filesystem::path> seen_paths;
    for (std::size_t index{}; index < artifacts.size(); ++index) {
        const Json &artifact = artifacts.at(index);
        if (!artifact.is_object()) {
            return parse_error(TrendManifestParseErrorCode::wrong_type,
                               "$.artifacts[" + std::to_string(index) + "]",
                               "Each artifact must be an object.");
        }
        if (!artifact.contains("path")) {
            return parse_error(TrendManifestParseErrorCode::missing_required_field,
                               "$.artifacts[" + std::to_string(index) + "]",
                               "Each artifact requires a path field.");
        }
        if (!has_exact_keys(artifact, {"path"})) {
            return parse_error(TrendManifestParseErrorCode::unknown_field,
                               "$.artifacts[" + std::to_string(index) + "]",
                               "Each artifact contains an unknown field.");
        }
        if (!artifact.at("path").is_string()) {
            return parse_error(TrendManifestParseErrorCode::wrong_type,
                               "$.artifacts[" + std::to_string(index) + "].path",
                               "The artifact path must be a string.");
        }

        auto path =
            parse_artifact_path(artifact.at("path").get<std::string>(), manifest.directory, index);
        if (!path.has_value()) {
            return std::unexpected(path.error());
        }
        if (!seen_paths.insert(*path).second) {
            return parse_error(TrendManifestParseErrorCode::duplicate_artifact_path,
                               "$.artifacts[" + std::to_string(index) + "].path",
                               "The artifact path is duplicated.");
        }
        manifest.artifact_paths.push_back(std::move(*path));
    }
    return manifest;
}

std::expected<TrendManifest, TrendManifestLoadError>
load_trend_manifest(const std::filesystem::path &manifest_path) {
    auto input = EvidenceInputReader::open(manifest_path, kTrendManifestMaximumBytes,
                                           kTrendManifestMaximumBytes);
    if (!input.has_value()) {
        return load_error(load_error_code(input.error().code), input.error().detail);
    }

    std::error_code size_error;
    const std::uintmax_t input_size = std::filesystem::file_size(manifest_path, size_error);
    if (size_error) {
        return load_error(TrendManifestLoadErrorCode::input_read_failed,
                          "Unable to determine the manifest size: " + size_error.message());
    }
    if (input_size > kTrendManifestMaximumBytes) {
        return load_error(TrendManifestLoadErrorCode::input_too_large,
                          "The manifest exceeds the configured size limit.");
    }

    std::string text;
    std::size_t physical_bytes{};
    while (true) {
        const auto line = input->next_line();
        if (!line.has_value()) {
            return load_error(load_error_code(line.error().code), line.error().detail);
        }
        if (!line->has_value()) {
            break;
        }
        const std::size_t delimiter_bytes = input->last_line_was_terminated() ? 1U : 0U;
        if (physical_bytes > kTrendManifestMaximumBytes - delimiter_bytes ||
            (**line).size() > kTrendManifestMaximumBytes - physical_bytes - delimiter_bytes) {
            return load_error(TrendManifestLoadErrorCode::input_too_large,
                              "The manifest exceeds the configured size limit.");
        }
        physical_bytes += delimiter_bytes + (**line).size();
        if (!text.empty()) {
            text.push_back('\n');
        }
        text += **line;
    }

    const auto directory = canonical_manifest_directory(manifest_path);
    if (!directory.has_value()) {
        return std::unexpected(directory.error());
    }
    auto parsed = parse_trend_manifest(text, *directory);
    if (!parsed.has_value()) {
        return load_error(TrendManifestLoadErrorCode::parse_failed,
                          parsed.error().json_path + ": " + parsed.error().detail);
    }
    // Lexical checks reject explicit traversal; canonical containment also rejects symlink escapes.
    const auto containment = validate_artifact_containment(*parsed);
    if (!containment.has_value()) {
        return std::unexpected(containment.error());
    }
    return std::move(*parsed);
}

} // namespace memoptima::trends
