#include "cli/formatting.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace memoptima::cli {
namespace {

TEST(Formatting, FormatsBytesAtUnitBoundaries) {
    EXPECT_EQ(format_bytes(0), "0 B");
    EXPECT_EQ(format_bytes(1023), "1023.00 B");
    EXPECT_EQ(format_bytes(1024), "1.00 KiB");
    EXPECT_EQ(format_bytes(1024ULL * 1024ULL), "1.00 MiB");
}

TEST(Formatting, FormatsPercentagesWithRequestedPrecision) {
    EXPECT_EQ(format_percentage(12.34, 1), "12.3%");
    EXPECT_EQ(format_percentage(12.34, 2), "12.34%");
}

TEST(Formatting, FormatsPressureDurationsWithStableUnits) {
    EXPECT_EQ(format_duration(999), "999 µs");
    EXPECT_EQ(format_duration(1000), "1.00 ms");
    EXPECT_EQ(format_duration(1000000), "1.00 s");
}

TEST(Formatting, FormatsOperationalDurationsAtUnitBoundaries) {
    using namespace std::chrono_literals;

    EXPECT_EQ(format_operational_duration(0ms), "< 1s");
    EXPECT_EQ(format_operational_duration(999ms), "< 1s");
    EXPECT_EQ(format_operational_duration(1s), "1s");
    EXPECT_EQ(format_operational_duration(59s + 999ms), "59s");
    EXPECT_EQ(format_operational_duration(60s), "1m 00s");
    EXPECT_EQ(format_operational_duration(60s + 999ms), "1m 00s");
    EXPECT_EQ(format_operational_duration(59min + 59s + 999ms), "59m 59s");
    EXPECT_EQ(format_operational_duration(1h), "1h 00m");
    EXPECT_EQ(format_operational_duration(1h + 59s + 999ms), "1h 00m");
    EXPECT_EQ(format_operational_duration(23h + 59min + 59s + 999ms), "23h 59m");
    EXPECT_EQ(format_operational_duration(24h), "1d 00h");
    EXPECT_EQ(format_operational_duration(3 * 24h + 4h + 59min + 999ms), "3d 04h");
}

TEST(Formatting, PreservesPrintableTerminalText) {
    EXPECT_EQ(escape_terminal_text("/docker/service-1_healthy"), "/docker/service-1_healthy");
}

TEST(Formatting, EscapesNamedTerminalControlsAndBackslashes) {
    EXPECT_EQ(escape_terminal_text("left\\right\nrow\rcolumn\tindent"),
              "left\\\\right\\nrow\\rcolumn\\tindent");
}

TEST(Formatting, EscapesOtherAsciiControlsWithUppercaseHexadecimal) {
    std::string input;
    input += '\0';
    input += '\x01';
    input += '\x1B';
    input += '[';
    input += '3';
    input += '1';
    input += 'm';
    input += '\x7F';

    EXPECT_EQ(escape_terminal_text(input), "\\x00\\x01\\x1B[31m\\x7F");
}

TEST(Formatting, EscapesUnicodeControlsAndPreservesPrintableUnicode) {
    const std::string input = "before" + std::string{"\xC2\x9B"} + std::string{"\xE2\x80\xAE"} +
                              std::string{"\xE2\x80\x8B"} + std::string{"\xE2\x80\xA8"} + " café";

    EXPECT_EQ(escape_terminal_text(input), "before\\u{9B}\\u{202E}\\u{200B}\\u{2028} café");
}

TEST(Formatting, EscapesEachInvalidUtf8ByteIndividually) {
    std::string input;
    input.push_back(static_cast<char>(0xC2U));
    input += 'A';
    input.push_back(static_cast<char>(0xE2U));
    input += '(';
    input.push_back(static_cast<char>(0xA1U));

    EXPECT_EQ(escape_terminal_text(input), "\\xC2A\\xE2(\\xA1");
}

} // namespace
} // namespace memoptima::cli
