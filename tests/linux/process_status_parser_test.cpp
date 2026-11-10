#include "linux/process_status_parser.hpp"

#include <gtest/gtest.h>

#include <string_view>

namespace memoptima::linux::detail {
namespace {

constexpr std::string_view kValidProcessStatus = R"(Name:	example
Pid:	42
VmSize:	  1000 kB
VmRSS:	   256 kB
)";

TEST(ProcessStatusParser, ParsesRequiredFieldsAndConvertsResidentMemoryToBytes) {
    const auto process = parse_process_status(42, kValidProcessStatus);

    ASSERT_TRUE(process.has_value());
    EXPECT_EQ(process->pid(), 42U);
    EXPECT_EQ(process->name(), "example");
    EXPECT_EQ(process->resident_memory_bytes(), 256ULL * 1024ULL);
}

TEST(ProcessStatusParser, RejectsMissingRequiredField) {
    const auto process = parse_process_status(42, "Name:\texample\n");

    ASSERT_FALSE(process.has_value());
    EXPECT_EQ(process.error().code, ProcessReadErrorCode::missing_required_field);
    EXPECT_EQ(process.error().field_name, "VmRSS");
}

TEST(ProcessStatusParser, RejectsDuplicateRequiredField) {
    const auto process = parse_process_status(42, R"(Name:	example
VmRSS:	256 kB
VmRSS:	128 kB
)");

    ASSERT_FALSE(process.has_value());
    EXPECT_EQ(process.error().code, ProcessReadErrorCode::duplicate_required_field);
    EXPECT_EQ(process.error().field_name, "VmRSS");
    EXPECT_EQ(process.error().line_number, 3U);
}

TEST(ProcessStatusParser, RejectsUnexpectedResidentMemoryUnit) {
    const auto process = parse_process_status(42, R"(Name:	example
VmRSS:	256 MiB
)");

    ASSERT_FALSE(process.has_value());
    EXPECT_EQ(process.error().code, ProcessReadErrorCode::unexpected_unit);
    EXPECT_EQ(process.error().field_name, "VmRSS");
}

TEST(ProcessStatusParser, RejectsResidentMemoryOverflow) {
    const auto process = parse_process_status(42, R"(Name:	example
VmRSS:	18446744073709551616 kB
)");

    ASSERT_FALSE(process.has_value());
    EXPECT_EQ(process.error().code, ProcessReadErrorCode::value_overflow);
    EXPECT_EQ(process.error().field_name, "VmRSS");
}

TEST(ProcessStatusParser, RejectsEmptyProcessName) {
    const auto process = parse_process_status(42, R"(Name:
VmRSS:	256 kB
)");

    ASSERT_FALSE(process.has_value());
    EXPECT_EQ(process.error().code, ProcessReadErrorCode::invalid_process_info);
    EXPECT_EQ(process.error().field_name, "Name");
}

}  // namespace
}  // namespace memoptima::linux::detail
