#include <memoptima/trends/trend_manifest.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

namespace memoptima::trends {
namespace {

TEST(TrendManifest, ParsesAndResolvesRelativeArtifactPaths) {
    const auto manifest = parse_trend_manifest(
        R"({"manifest_version":1,"artifacts":[{"path":"evidence/one.jsonl"},{"path":"two.jsonl"}]})",
        "/tmp/trend-set");

    ASSERT_TRUE(manifest.has_value());
    ASSERT_EQ(manifest->artifact_paths.size(), 2U);
    EXPECT_EQ(manifest->directory, "/tmp/trend-set");
    EXPECT_EQ(manifest->artifact_paths[0], "/tmp/trend-set/evidence/one.jsonl");
    EXPECT_EQ(manifest->artifact_paths[1], "/tmp/trend-set/two.jsonl");
}

TEST(TrendManifest, RejectsClosedSchemaViolations) {
    const auto unknown = parse_trend_manifest(
        R"({"manifest_version":1,"artifacts":[{"path":"one.jsonl"}],"extra":true})", "/tmp");
    const auto missing = parse_trend_manifest(R"({"manifest_version":1})", "/tmp");
    const auto wrong_path_type =
        parse_trend_manifest(R"({"manifest_version":1,"artifacts":[{"path":null}]})", "/tmp");

    ASSERT_FALSE(unknown.has_value());
    ASSERT_FALSE(missing.has_value());
    ASSERT_FALSE(wrong_path_type.has_value());
    EXPECT_EQ(unknown.error().code, TrendManifestParseErrorCode::unknown_field);
    EXPECT_EQ(missing.error().code, TrendManifestParseErrorCode::missing_required_field);
    EXPECT_EQ(wrong_path_type.error().code, TrendManifestParseErrorCode::wrong_type);
}

TEST(TrendManifest, RejectsDuplicateKeysBeforeSchemaValidation) {
    const auto artifacts = parse_trend_manifest(
        R"({"manifest_version":1,"artifacts":[],"artifacts":[{"path":"other.jsonl"}]})", "/tmp");
    const auto version = parse_trend_manifest(
        R"({"manifest_version":1,"manifest_version":2,"artifacts":[{"path":"one.jsonl"}]})",
        "/tmp");
    const auto path = parse_trend_manifest(
        R"({"manifest_version":1,"artifacts":[{"path":"one.jsonl"},{"path":"two.jsonl","path":"three.jsonl"}]})",
        "/tmp");
    const auto escaped_path = parse_trend_manifest(
        R"({"manifest_version":1,"artifacts":[{"path":"one.jsonl","p\u0061th":"two.jsonl"}]})",
        "/tmp");

    ASSERT_FALSE(artifacts.has_value());
    ASSERT_FALSE(version.has_value());
    ASSERT_FALSE(path.has_value());
    ASSERT_FALSE(escaped_path.has_value());
    EXPECT_EQ(artifacts.error().code, TrendManifestParseErrorCode::duplicate_key);
    EXPECT_EQ(artifacts.error().json_path, "$.artifacts");
    EXPECT_EQ(version.error().code, TrendManifestParseErrorCode::duplicate_key);
    EXPECT_EQ(version.error().json_path, "$.manifest_version");
    EXPECT_EQ(path.error().code, TrendManifestParseErrorCode::duplicate_key);
    EXPECT_EQ(path.error().json_path, "$.artifacts[1].path");
    EXPECT_EQ(escaped_path.error().code, TrendManifestParseErrorCode::duplicate_key);
    EXPECT_EQ(escaped_path.error().json_path, "$.artifacts[0].path");
}

TEST(TrendManifest, EnforcesTheStrictJsonNestingLimit) {
    const std::string arrays_63(63U, '[');
    const std::string closing_63(63U, ']');
    const std::string arrays_64(64U, '[');
    const std::string closing_64(64U, ']');
    const auto within_limit =
        parse_trend_manifest(R"({"value":)" + arrays_63 + "0" + closing_63 + "}", "/tmp");
    const auto too_deep =
        parse_trend_manifest(R"({"value":)" + arrays_64 + "0" + closing_64 + "}", "/tmp");

    ASSERT_FALSE(within_limit.has_value());
    ASSERT_FALSE(too_deep.has_value());
    EXPECT_EQ(within_limit.error().code, TrendManifestParseErrorCode::missing_required_field);
    EXPECT_EQ(too_deep.error().code, TrendManifestParseErrorCode::nesting_too_deep);
    EXPECT_EQ(too_deep.error().json_path, "$");
}

TEST(TrendManifest, GivesDuplicateKeyPrecedenceOverDuplicateArtifactPath) {
    const auto manifest = parse_trend_manifest(
        R"({"manifest_version":1,"artifacts":[{"path":"one.jsonl","path":"two.jsonl"},{"path":"one.jsonl"}]})",
        "/tmp");

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().code, TrendManifestParseErrorCode::duplicate_key);
    EXPECT_EQ(manifest.error().json_path, "$.artifacts[0].path");
}

TEST(TrendManifest, RejectsUnsafeOrDuplicatePaths) {
    const auto absolute = parse_trend_manifest(
        R"({"manifest_version":1,"artifacts":[{"path":"/tmp/one.jsonl"}]})", "/tmp");
    const auto parent = parse_trend_manifest(
        R"({"manifest_version":1,"artifacts":[{"path":"../one.jsonl"}]})", "/tmp");
    const auto duplicate = parse_trend_manifest(
        R"({"manifest_version":1,"artifacts":[{"path":"one.jsonl"},{"path":"./one.jsonl"}]})",
        "/tmp");

    ASSERT_FALSE(absolute.has_value());
    ASSERT_FALSE(parent.has_value());
    ASSERT_FALSE(duplicate.has_value());
    EXPECT_EQ(absolute.error().code, TrendManifestParseErrorCode::invalid_artifact_path);
    EXPECT_EQ(parent.error().code, TrendManifestParseErrorCode::invalid_artifact_path);
    EXPECT_EQ(duplicate.error().code, TrendManifestParseErrorCode::duplicate_artifact_path);
}

TEST(TrendManifest, RejectsArtifactPathsContainingNulBytes) {
    const auto manifest = parse_trend_manifest(
        R"({"manifest_version":1,"artifacts":[{"path":"safe\u0000/../../outside.jsonl"}]})",
        "/tmp");

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().code, TrendManifestParseErrorCode::invalid_artifact_path);
    EXPECT_EQ(manifest.error().json_path, "$.artifacts[0].path");
    EXPECT_EQ(manifest.error().detail, "The artifact path must not contain a NUL byte.");
}

TEST(TrendManifest, EnforcesArtifactCountLimit) {
    std::string text = R"({"manifest_version":1,"artifacts":[)";
    for (std::size_t index{}; index <= kTrendManifestMaximumArtifacts; ++index) {
        if (index != 0U) {
            text += ',';
        }
        text += R"({"path":"artifact-)" + std::to_string(index) + R"(.jsonl"})";
    }
    text += "]}";

    const auto manifest = parse_trend_manifest(text, "/tmp");

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().code, TrendManifestParseErrorCode::invalid_artifact_count);
}

TEST(TrendManifest, LoaderPreservesDuplicateKeyParseDiagnostics) {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("memoptima-trend-manifest-" + std::to_string(::getpid()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    const auto manifest_path = directory / "manifest.json";
    {
        std::ofstream output{manifest_path};
        ASSERT_TRUE(output.is_open());
        output << R"({"manifest_version":1,"artifacts":[],"artifacts":[{"path":"other.jsonl"}]})";
    }

    const auto result = load_trend_manifest(manifest_path);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TrendManifestLoadErrorCode::parse_failed);
    EXPECT_NE(result.error().detail.find("$.artifacts: The key occurs more than once"),
              std::string::npos);
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    EXPECT_FALSE(cleanup_error) << cleanup_error.message();
}

TEST(TrendManifest, LoaderRejectsSymbolicLinks) {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("memoptima-trend-manifest-" + std::to_string(::getpid()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    const auto target = directory / "target.json";
    const auto link = directory / "manifest.json";
    {
        std::ofstream output{target};
        ASSERT_TRUE(output.is_open());
        output << R"({"manifest_version":1,"artifacts":[{"path":"artifact.jsonl"}]})";
    }
    std::filesystem::create_symlink(target, link);

    const auto result = load_trend_manifest(link);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TrendManifestLoadErrorCode::input_is_symbolic_link);
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    EXPECT_FALSE(cleanup_error) << cleanup_error.message();
}

TEST(TrendManifest, LoaderRejectsAManifestDirectoryPathContainingASymbolicLink) {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("memoptima-trend-manifest-" + std::to_string(::getpid()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto actual_directory = directory / "actual";
    const auto alias_directory = directory / "alias";
    ASSERT_TRUE(std::filesystem::create_directories(actual_directory));
    std::filesystem::create_directory_symlink(actual_directory, alias_directory);
    const auto manifest_path = actual_directory / "manifest.json";
    {
        std::ofstream output{manifest_path};
        ASSERT_TRUE(output.is_open());
        output << R"({"manifest_version":1,"artifacts":[{"path":"artifact.jsonl"}]})";
    }
    {
        std::ofstream output{actual_directory / "artifact.jsonl"};
        ASSERT_TRUE(output.is_open());
        output << "{}\n";
    }

    const auto result = load_trend_manifest(alias_directory / "manifest.json");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TrendManifestLoadErrorCode::input_is_symbolic_link);
    EXPECT_EQ(result.error().detail,
              "The manifest directory path must not contain a symbolic link.");
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    EXPECT_FALSE(cleanup_error) << cleanup_error.message();
}

TEST(TrendManifest, LoaderAcceptsBareAndExplicitRelativeManifestPaths) {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("memoptima-trend-manifest-" + std::to_string(::getpid()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    {
        std::ofstream output{directory / "manifest.json"};
        ASSERT_TRUE(output.is_open());
        output << R"({"manifest_version":1,"artifacts":[{"path":"artifact.jsonl"}]})";
    }
    {
        std::ofstream output{directory / "artifact.jsonl"};
        ASSERT_TRUE(output.is_open());
        output << "{}\n";
    }

    const std::filesystem::path original_directory = std::filesystem::current_path();
    std::filesystem::current_path(directory);
    const auto bare = load_trend_manifest("manifest.json");
    const auto explicit_relative = load_trend_manifest("./manifest.json");
    std::filesystem::current_path(original_directory);

    ASSERT_TRUE(bare.has_value());
    ASSERT_TRUE(explicit_relative.has_value());
    EXPECT_EQ(bare->directory, directory);
    EXPECT_EQ(explicit_relative->directory, directory);
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    EXPECT_FALSE(cleanup_error) << cleanup_error.message();
}

TEST(TrendManifest, LoaderRejectsASymbolicDirectoryBeforeParentTraversalNormalizesIt) {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("memoptima-trend-manifest-" + std::to_string(::getpid()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto actual_directory = directory / "actual";
    const auto alias_directory = directory / "alias";
    ASSERT_TRUE(std::filesystem::create_directories(actual_directory));
    std::filesystem::create_directory_symlink(actual_directory, alias_directory);
    const auto manifest_path = actual_directory / "manifest.json";
    {
        std::ofstream output{manifest_path};
        ASSERT_TRUE(output.is_open());
        output << R"({"manifest_version":1,"artifacts":[{"path":"artifact.jsonl"}]})";
    }
    {
        std::ofstream output{actual_directory / "artifact.jsonl"};
        ASSERT_TRUE(output.is_open());
        output << "{}\n";
    }

    const auto result = load_trend_manifest(alias_directory / ".." / "actual" / "manifest.json");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TrendManifestLoadErrorCode::input_is_symbolic_link);
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    EXPECT_FALSE(cleanup_error) << cleanup_error.message();
}

TEST(TrendManifest, LoaderBoundsTheAggregateManifestSize) {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("memoptima-trend-manifest-" + std::to_string(::getpid()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    const auto manifest_path = directory / "manifest.json";
    {
        std::ofstream output{manifest_path};
        ASSERT_TRUE(output.is_open());
        output << std::string(kTrendManifestMaximumBytes / 2U + 1U, ' ') << '\n'
               << std::string(kTrendManifestMaximumBytes / 2U, ' ');
    }

    const auto result = load_trend_manifest(manifest_path);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TrendManifestLoadErrorCode::input_too_large);
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    EXPECT_FALSE(cleanup_error) << cleanup_error.message();
}

TEST(TrendManifest, LoaderCountsLeadingBlankLinesTowardTheSizeLimit) {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("memoptima-trend-manifest-" + std::to_string(::getpid()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    const auto manifest_path = directory / "manifest.json";
    {
        std::ofstream output{manifest_path};
        ASSERT_TRUE(output.is_open());
        output << std::string(kTrendManifestMaximumBytes + 1U, '\n')
               << R"({"manifest_version":1,"artifacts":[{"path":"artifact.jsonl"}]})";
    }

    const auto result = load_trend_manifest(manifest_path);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TrendManifestLoadErrorCode::input_too_large);
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    EXPECT_FALSE(cleanup_error) << cleanup_error.message();
}

TEST(TrendManifest, LoaderRejectsDataAfterAnExactlyFullLine) {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("memoptima-trend-manifest-" + std::to_string(::getpid()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    const auto manifest_path = directory / "manifest.json";
    {
        std::ofstream output{manifest_path};
        ASSERT_TRUE(output.is_open());
        output << std::string(kTrendManifestMaximumBytes, ' ') << '\n';
    }

    const auto result = load_trend_manifest(manifest_path);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TrendManifestLoadErrorCode::input_too_large);
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    EXPECT_FALSE(cleanup_error) << cleanup_error.message();
}

TEST(TrendManifest, LoaderAcceptsAnExactlyMaximumSizeUnterminatedManifest) {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("memoptima-trend-manifest-" + std::to_string(::getpid()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    const auto manifest_path = directory / "manifest.json";
    std::string manifest = R"({"manifest_version":1,"artifacts":[{"path":"artifact.jsonl"}]})";
    manifest.append(kTrendManifestMaximumBytes - manifest.size(), ' ');
    {
        std::ofstream output{manifest_path};
        ASSERT_TRUE(output.is_open());
        output << manifest;
    }

    const auto result = load_trend_manifest(manifest_path);

    ASSERT_TRUE(result.has_value());
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    EXPECT_FALSE(cleanup_error) << cleanup_error.message();
}

TEST(TrendManifest, LoaderRejectsArtifactPathEscapingThroughAnIntermediateLink) {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("memoptima-trend-manifest-" + std::to_string(::getpid()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto outside = directory.parent_path() / (directory.filename().string() + "-outside");
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    ASSERT_TRUE(std::filesystem::create_directory(outside));
    std::filesystem::create_directory_symlink(outside, directory / "evidence");
    const auto manifest_path = directory / "manifest.json";
    {
        std::ofstream output{manifest_path};
        ASSERT_TRUE(output.is_open());
        output << R"({"manifest_version":1,"artifacts":[{"path":"evidence/session.jsonl"}]})";
    }

    const auto result = load_trend_manifest(manifest_path);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, TrendManifestLoadErrorCode::parse_failed);
    EXPECT_NE(result.error().detail.find("$.artifacts[0].path"), std::string::npos);
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    EXPECT_FALSE(cleanup_error) << cleanup_error.message();
    std::filesystem::remove_all(outside, cleanup_error);
    EXPECT_FALSE(cleanup_error) << cleanup_error.message();
}

} // namespace
} // namespace memoptima::trends
