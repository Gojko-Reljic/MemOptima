#include <memoptima/application_info.hpp>

#include <gtest/gtest.h>

#include <string_view>

namespace memoptima {
namespace {

TEST(ApplicationInfo, ExposesTheProductIdentity) {
    EXPECT_EQ(application_name(), "MemOptima");
    EXPECT_EQ(application_description(), "Adaptive Memory Manager for Linux");
}

TEST(ApplicationInfo, ExposesANonEmptyVersion) { EXPECT_FALSE(version().empty()); }

} // namespace
} // namespace memoptima
