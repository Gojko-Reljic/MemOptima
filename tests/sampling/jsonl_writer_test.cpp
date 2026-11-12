#include <memoptima/sampling/jsonl_writer.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

namespace memoptima::sampling {
namespace {

class JsonlWriterTest : public testing::Test {
  protected:
    void SetUp() override {
        const auto unique_suffix =
            std::to_string(::getpid()) + "-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        directory_ = std::filesystem::temp_directory_path() / ("memoptima-jsonl-" + unique_suffix);
        ASSERT_TRUE(std::filesystem::create_directory(directory_));
    }

    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
        EXPECT_FALSE(error) << error.message();
    }

    [[nodiscard]] std::filesystem::path output_path() const {
        return directory_ / "evidence.jsonl";
    }

  private:
    std::filesystem::path directory_;
};

TEST_F(JsonlWriterTest, CreatesOwnerOnlyOutputAndFlushesCompleteRecords) {
    const std::filesystem::path output = output_path();
    auto writer = SamplingJsonlWriter::create(output);

    ASSERT_TRUE(writer.has_value());
    ASSERT_TRUE(writer->write_record("{\"record_type\":\"session_start\"}").has_value());
    ASSERT_TRUE(writer->write_record("{\"record_type\":\"session_end\"}").has_value());
    EXPECT_EQ(writer->committed_size_bytes(), 62U);
    ASSERT_TRUE(writer->close().has_value());

    struct stat status{};
    ASSERT_EQ(::stat(output.c_str(), &status), 0);
    EXPECT_EQ(status.st_mode & 0777, 0600);

    std::ifstream input{output};
    std::string content{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    EXPECT_EQ(content, "{\"record_type\":\"session_start\"}\n{\"record_type\":\"session_end\"}\n");
}

TEST_F(JsonlWriterTest, RefusesToOverwriteAnExistingOutput) {
    const std::filesystem::path output = output_path();
    {
        std::ofstream existing{output};
        existing << "preserve this artifact";
    }

    const auto writer = SamplingJsonlWriter::create(output);

    ASSERT_FALSE(writer.has_value());
    EXPECT_EQ(writer.error().code, SamplingJsonlWriterErrorCode::output_already_exists);
    std::ifstream input{output};
    std::string content{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    EXPECT_EQ(content, "preserve this artifact");
}

TEST_F(JsonlWriterTest, RejectsLineBreaksAndPreservesTerminalSpace) {
    auto writer = SamplingJsonlWriter::create(output_path());
    ASSERT_TRUE(writer.has_value());

    const auto invalid = writer->write_record("{\n}");
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error().code, SamplingJsonlWriterErrorCode::invalid_record);

    ASSERT_TRUE(writer->write_record("{}", kSamplingOutputMaximumBytes - 3).has_value());
    const auto exhausted = writer->write_record("{}", kSamplingOutputMaximumBytes - 3);
    ASSERT_FALSE(exhausted.has_value());
    EXPECT_EQ(exhausted.error().code, SamplingJsonlWriterErrorCode::output_size_limit_reached);
}

} // namespace
} // namespace memoptima::sampling
