#pragma once

#include <filesystem>
#include <optional>

namespace memoptima::trends::trend_path {

// Keeps cataloging and replay on the same descriptor-relative artifact-path boundary.
[[nodiscard]] inline std::optional<std::filesystem::path>
relative_artifact_path(const std::filesystem::path &manifest_directory,
                       const std::filesystem::path &artifact_path) {
    const std::filesystem::path relative = artifact_path.lexically_relative(manifest_directory);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
        return std::nullopt;
    }
    return relative;
}

} // namespace memoptima::trends::trend_path
