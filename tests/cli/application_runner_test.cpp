#include "cli/application_runner.hpp"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <system_error>
#include <unistd.h>

namespace memoptima::cli {
namespace {

class RejectingOutputBuffer final : public std::streambuf {
  protected:
    int_type overflow(int_type) override { return traits_type::eof(); }

    std::streamsize xsputn(const char *, std::streamsize) override { return 0; }
};

class DeferredFailureOutputBuffer final : public std::stringbuf {
  protected:
    int sync() override { return -1; }
};

class ScopedStandardStreamBuffers final {
  public:
    ScopedStandardStreamBuffers(std::streambuf *standard_output, std::streambuf *standard_error)
        : original_output_{std::cout.rdbuf(standard_output)},
          original_error_{std::cerr.rdbuf(standard_error)} {}

    ~ScopedStandardStreamBuffers() {
        std::cout.rdbuf(original_output_);
        std::cerr.rdbuf(original_error_);
        std::cout.clear();
        std::cerr.clear();
    }

    ScopedStandardStreamBuffers(const ScopedStandardStreamBuffers &) = delete;
    ScopedStandardStreamBuffers &operator=(const ScopedStandardStreamBuffers &) = delete;

  private:
    std::streambuf *original_output_;
    std::streambuf *original_error_;
};

class ApplicationRunnerTest : public testing::Test {
  protected:
    void SetUp() override {
        directory_ = std::filesystem::temp_directory_path() /
                     ("memoptima-application-runner-" + std::to_string(::getpid()) + "-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory_));
    }

    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
        EXPECT_FALSE(error) << error.message();
    }

    [[nodiscard]] std::filesystem::path write_empty_trend_manifest() const {
        const auto artifact = directory_ / "evidence.jsonl";
        {
            std::ofstream output{artifact};
            EXPECT_TRUE(output.is_open());
            output
                << R"({"record_type":"session_start","schema_version":1,"application_version":"0.7.0","started_at_utc":"2026-01-01T00:00:00.000Z","requested_sample_count":3,"requested_interval_ms":"10000","output_max_bytes":"536870912","output_permissions_octal":"0600","flush_after_each_record":true,"host_identifiers_included":false})"
                << '\n'
                << R"({"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"2026-01-01T00:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[]})"
                << '\n'
                << R"({"record_type":"sample","schema_version":1,"sample_sequence":2,"planned_offset_ms":"10000","captured_at_utc":"2026-01-01T00:00:10.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[]})"
                << '\n'
                << R"({"record_type":"sample","schema_version":1,"sample_sequence":3,"planned_offset_ms":"20000","captured_at_utc":"2026-01-01T00:00:20.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[]})"
                << '\n'
                << R"({"record_type":"session_end","schema_version":1,"outcome":"completed","completed_sample_count":3,"ended_at_utc":"2026-01-01T00:00:20.001Z","failure":null})"
                << '\n';
        }
        const auto manifest = directory_ / "manifest.json";
        std::ofstream output{manifest};
        EXPECT_TRUE(output.is_open());
        output << R"({"manifest_version":1,"artifacts":[{"path":"evidence.jsonl"}]})";
        return manifest;
    }

    std::filesystem::path directory_;
};

TEST_F(ApplicationRunnerTest, ReportsAStandardOutputFailureForTrendAnalysis) {
    const auto manifest = write_empty_trend_manifest();
    RejectingOutputBuffer rejecting_output;
    std::ostringstream standard_error;
    const ScopedStandardStreamBuffers redirected{&rejecting_output, standard_error.rdbuf()};

    std::array<std::string, 4U> argument_storage{"memoptima", "trends", "analyze",
                                                 manifest.string()};
    std::array<char *, 4U> arguments{};
    for (std::size_t index{}; index < arguments.size(); ++index)
        arguments[index] = argument_storage[index].data();

    EXPECT_EQ(run_application(static_cast<int>(arguments.size()), arguments.data()), 1);
    EXPECT_EQ(standard_error.str(),
              "Error: unable to write trend analysis table to standard output.\n");
}

TEST_F(ApplicationRunnerTest, ReportsADeferredStandardOutputFailureForTrendAnalysis) {
    const auto manifest = write_empty_trend_manifest();
    DeferredFailureOutputBuffer deferred_failure_output;
    std::ostringstream standard_error;
    const ScopedStandardStreamBuffers redirected{&deferred_failure_output, standard_error.rdbuf()};

    std::array<std::string, 4U> argument_storage{"memoptima", "trends", "analyze",
                                                 manifest.string()};
    std::array<char *, 4U> arguments{};
    for (std::size_t index{}; index < arguments.size(); ++index)
        arguments[index] = argument_storage[index].data();

    EXPECT_EQ(run_application(static_cast<int>(arguments.size()), arguments.data()), 1);
    EXPECT_EQ(standard_error.str(),
              "Error: unable to write trend analysis table to standard output.\n");
}

} // namespace
} // namespace memoptima::cli
