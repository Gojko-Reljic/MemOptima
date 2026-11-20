#include <memoptima/evidence_input_reader.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <sys/stat.h>
#include <unistd.h>

namespace memoptima {
namespace {

class EvidenceInputReaderTest : public testing::Test {
  protected:
    void SetUp() override {
        const std::string unique_suffix =
            std::to_string(::getpid()) + "-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        directory_ =
            std::filesystem::temp_directory_path() / ("memoptima-evidence-" + unique_suffix);
        ASSERT_TRUE(std::filesystem::create_directory(directory_));
    }

    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
        EXPECT_FALSE(error) << error.message();
    }

    [[nodiscard]] std::filesystem::path path(std::string_view name) const {
        return directory_ / name;
    }

    void write_file(const std::filesystem::path &file, std::string_view contents) {
        std::ofstream output{file, std::ios::binary};
        ASSERT_TRUE(output.is_open());
        output << contents;
        ASSERT_TRUE(output.good());
    }

  private:
    std::filesystem::path directory_;
};

TEST_F(EvidenceInputReaderTest, ReadsPhysicalLinesAndFinalUnterminatedLine) {
    const std::filesystem::path input = path("evidence.jsonl");
    write_file(input, "first\nsecond\nthird");

    auto reader = EvidenceInputReader::open(input);
    ASSERT_TRUE(reader.has_value());

    const auto first = reader->next_line();
    const bool first_was_terminated = reader->last_line_was_terminated();
    const auto second = reader->next_line();
    const bool second_was_terminated = reader->last_line_was_terminated();
    const auto third = reader->next_line();
    const bool third_was_terminated = reader->last_line_was_terminated();
    const auto end = reader->next_line();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(third.has_value());
    ASSERT_TRUE(end.has_value());
    EXPECT_EQ(*first, "first");
    EXPECT_EQ(*second, "second");
    EXPECT_EQ(*third, "third");
    EXPECT_TRUE(first_was_terminated);
    EXPECT_TRUE(second_was_terminated);
    EXPECT_FALSE(third_was_terminated);
    EXPECT_FALSE(end->has_value());
}

TEST_F(EvidenceInputReaderTest, RejectsSymbolicLinksDirectoriesAndFifos) {
    const std::filesystem::path target = path("target.jsonl");
    const std::filesystem::path link = path("link.jsonl");
    const std::filesystem::path fifo = path("input.fifo");
    write_file(target, "{}\n");
    std::filesystem::create_symlink(target, link);
    ASSERT_EQ(::mkfifo(fifo.c_str(), S_IRUSR | S_IWUSR), 0);

    const auto linked_reader = EvidenceInputReader::open(link);
    const auto directory_reader = EvidenceInputReader::open(path(""));
    const auto fifo_reader = EvidenceInputReader::open(fifo);

    ASSERT_FALSE(linked_reader.has_value());
    ASSERT_FALSE(directory_reader.has_value());
    ASSERT_FALSE(fifo_reader.has_value());
    EXPECT_EQ(linked_reader.error().code, EvidenceInputErrorCode::input_is_symbolic_link);
    EXPECT_EQ(directory_reader.error().code, EvidenceInputErrorCode::input_not_regular_file);
    EXPECT_EQ(fifo_reader.error().code, EvidenceInputErrorCode::input_not_regular_file);
}

TEST_F(EvidenceInputReaderTest, StopsBeforeAcceptingALineBeyondItsConfiguredLimit) {
    const std::filesystem::path input = path("too-long.jsonl");
    write_file(input, "abcd\n");

    auto reader = EvidenceInputReader::open(input, 3);
    ASSERT_TRUE(reader.has_value());
    const auto line = reader->next_line();

    ASSERT_FALSE(line.has_value());
    EXPECT_EQ(line.error().code, EvidenceInputErrorCode::line_too_long);
}

TEST_F(EvidenceInputReaderTest, AcceptsInputAtTheExactAggregateLimit) {
    const std::filesystem::path input = path("exact-limit.jsonl");
    write_file(input, "ab\nc");

    auto reader = EvidenceInputReader::open(input, 3, 4);
    ASSERT_TRUE(reader.has_value());
    const auto first = reader->next_line();
    const auto second = reader->next_line();
    const auto end = reader->next_line();
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(end.has_value());
    EXPECT_EQ(*first, "ab");
    EXPECT_EQ(*second, "c");
    EXPECT_FALSE(end->has_value());
}

TEST_F(EvidenceInputReaderTest, RejectsArtifactOneByteAboveTheAggregateLimitBeforeReading) {
    const std::filesystem::path input = path("over-limit.jsonl");
    write_file(input, "abcde");

    const auto reader = EvidenceInputReader::open(input, 5, 4);

    ASSERT_FALSE(reader.has_value());
    EXPECT_EQ(reader.error().code, EvidenceInputErrorCode::input_too_large);
}

TEST_F(EvidenceInputReaderTest, RejectsAggregateInputBeyondTheConfiguredLimit) {
    const std::filesystem::path input = path("aggregate-limit.jsonl");
    write_file(input, "ab\n");

    auto reader = EvidenceInputReader::open(input, 3, 5);
    ASSERT_TRUE(reader.has_value());
    {
        std::ofstream output{input, std::ios::binary | std::ios::app};
        ASSERT_TRUE(output.is_open());
        output << "cd\n";
        ASSERT_TRUE(output.good());
    }
    const auto first = reader->next_line();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, "ab");
    const auto second = reader->next_line();

    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error().code, EvidenceInputErrorCode::input_too_large);
}

} // namespace
} // namespace memoptima
