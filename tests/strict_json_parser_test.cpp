#include "strict_json_parser.hpp"

#include <gtest/gtest.h>

#include <string>

namespace memoptima::detail {
namespace {

TEST(StrictJsonParser, ParsesValidJson) {
    const auto parsed = parse_strict_json(R"({"containers":[{"id":"a"}]})");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed)["containers"][0]["id"], "a");
}

TEST(StrictJsonParser, RejectsTopLevelDuplicateKey) {
    const auto parsed = parse_strict_json(R"({"record_type":"sample","record_type":"sample"})");

    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error().kind, StrictJsonErrorKind::duplicate_key);
    ASSERT_TRUE(parsed.error().duplicate.has_value());
    EXPECT_EQ(parsed.error().duplicate->object_path, "$");
    EXPECT_EQ(parsed.error().duplicate->key, "record_type");
}

TEST(StrictJsonParser, RejectsNestedDuplicateKeyWithArrayPath) {
    const auto parsed = parse_strict_json(
        R"({"containers":[{"memory":{"current_bytes":"1","current_bytes":"2"}}]})");

    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error().kind, StrictJsonErrorKind::duplicate_key);
    ASSERT_TRUE(parsed.error().duplicate.has_value());
    EXPECT_EQ(parsed.error().duplicate->object_path, "$.containers[0].memory");
    EXPECT_EQ(parsed.error().duplicate->key, "current_bytes");
}

TEST(StrictJsonParser, DetectsDuplicatesAfterJsonEscapeDecoding) {
    const auto parsed = parse_strict_json(R"({"a":1,"\u0061":2})");

    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error().kind, StrictJsonErrorKind::duplicate_key);
    ASSERT_TRUE(parsed.error().duplicate.has_value());
    EXPECT_EQ(parsed.error().duplicate->key, "a");
}

TEST(StrictJsonParser, DoesNotTreatKeysInSeparateObjectsAsDuplicates) {
    const auto parsed = parse_strict_json(R"({"first":{"id":1},"second":{"id":2}})");

    EXPECT_TRUE(parsed.has_value());
}

TEST(StrictJsonParser, DoesNotTreatKeysAtDifferentDepthsAsDuplicates) {
    const auto parsed = parse_strict_json(R"({"id":1,"nested":{"id":2}})");

    EXPECT_TRUE(parsed.has_value());
}

TEST(StrictJsonParser, UsesUnambiguousPathsForNonIdentifierKeys) {
    const auto dotted = parse_strict_json(R"({"a.b":{"value":1,"value":2}})");
    const auto empty = parse_strict_json(R"({"":{"value":1,"value":2}})");

    ASSERT_FALSE(dotted.has_value());
    ASSERT_FALSE(empty.has_value());
    ASSERT_TRUE(dotted.error().duplicate.has_value());
    ASSERT_TRUE(empty.error().duplicate.has_value());
    EXPECT_EQ(dotted.error().duplicate->object_path, R"($["a.b"])");
    EXPECT_EQ(empty.error().duplicate->object_path, R"($[""])");
}

TEST(StrictJsonParser, GivesInvalidJsonPrecedenceOverDuplicateKey) {
    const auto parsed = parse_strict_json(R"({"a":1,"a":2)");

    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error().kind, StrictJsonErrorKind::invalid_json);
}

TEST(StrictJsonParser, AllowsExactlyMaximumNestingDepth) {
    const std::string input(kStrictJsonMaximumNestingDepth, '[');
    const std::string closing(kStrictJsonMaximumNestingDepth, ']');
    const auto parsed = parse_strict_json(input + "0" + closing);

    EXPECT_TRUE(parsed.has_value());
}

TEST(StrictJsonParser, RejectsNestingBeyondMaximumDepth) {
    const std::string input(kStrictJsonMaximumNestingDepth + 1U, '[');
    const std::string closing(kStrictJsonMaximumNestingDepth + 1U, ']');
    const auto parsed = parse_strict_json(input + "0" + closing);

    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error().kind, StrictJsonErrorKind::nesting_too_deep);
}

TEST(StrictJsonParser, IgnoresBracketsWithinStringsDuringNestingPrescan) {
    const auto parsed = parse_strict_json(R"({"text":"[ { ] } and \\\" still quoted"})");

    EXPECT_TRUE(parsed.has_value());
}

TEST(StrictJsonParser, GivesNestingLimitPrecedenceOverInvalidJson) {
    const std::string input(kStrictJsonMaximumNestingDepth + 1U, '[');
    const auto parsed = parse_strict_json(input);

    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error().kind, StrictJsonErrorKind::nesting_too_deep);
}

} // namespace
} // namespace memoptima::detail
