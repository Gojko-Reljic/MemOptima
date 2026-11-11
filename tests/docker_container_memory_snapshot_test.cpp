#include <memoptima/docker_container_memory_snapshot.hpp>

#include <gtest/gtest.h>

namespace memoptima {
namespace {

TEST(DockerContainerIdTest, NormalizesUppercaseHexadecimalCharacters) {
    const std::string uppercase_id(64, 'A');

    const auto parsed = DockerContainerId::parse(uppercase_id);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->value(), std::string(64, 'a'));
}

TEST(DockerContainerIdTest, RejectsAnIdentifierWithTheWrongLength) {
    const auto parsed = DockerContainerId::parse("abc");

    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error().code, DockerContainerIdParseErrorCode::incorrect_length);
}

TEST(DockerContainerIdTest, RejectsANonHexadecimalIdentifier) {
    std::string invalid_id(64, 'a');
    invalid_id[32] = 'z';

    const auto parsed = DockerContainerId::parse(invalid_id);

    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error().code, DockerContainerIdParseErrorCode::invalid_character);
}

} // namespace
} // namespace memoptima
