#include <memoptima/process_memory_info.hpp>

#include <gtest/gtest.h>

namespace memoptima {
namespace {

TEST(ProcessMemoryInfo, CreatesAValidProcess) {
    const auto process = ProcessMemoryInfo::create(42, "example", 1024);

    ASSERT_TRUE(process.has_value());
    EXPECT_EQ(process->pid(), 42U);
    EXPECT_EQ(process->name(), "example");
    EXPECT_EQ(process->resident_memory_bytes(), 1024U);
}

TEST(ProcessMemoryInfo, RejectsZeroPid) {
    const auto process = ProcessMemoryInfo::create(0, "example", 1024);

    ASSERT_FALSE(process.has_value());
    EXPECT_EQ(process.error().code, ProcessInfoValidationErrorCode::pid_must_be_positive);
}

TEST(ProcessMemoryInfo, RejectsEmptyName) {
    const auto process = ProcessMemoryInfo::create(42, "", 1024);

    ASSERT_FALSE(process.has_value());
    EXPECT_EQ(process.error().code, ProcessInfoValidationErrorCode::name_must_not_be_empty);
}

}  // namespace
}  // namespace memoptima
