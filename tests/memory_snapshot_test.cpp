#include <memoptima/memory_snapshot.hpp>

#include <gtest/gtest.h>

namespace memoptima {
namespace {

TEST(MemorySnapshot, CreatesAValidSnapshot) {
    const auto snapshot = MemorySnapshot::create(100, 40, 20, 5);

    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->total_memory_bytes(), 100U);
    EXPECT_EQ(snapshot->available_memory_bytes(), 40U);
    EXPECT_EQ(snapshot->total_swap_bytes(), 20U);
    EXPECT_EQ(snapshot->free_swap_bytes(), 5U);
}

TEST(MemorySnapshot, RejectsZeroTotalMemory) {
    const auto snapshot = MemorySnapshot::create(0, 0, 20, 5);

    ASSERT_FALSE(snapshot.has_value());
    EXPECT_EQ(snapshot.error().code, SnapshotValidationErrorCode::total_memory_must_be_positive);
}

TEST(MemorySnapshot, RejectsAvailableMemoryAboveTotalMemory) {
    const auto snapshot = MemorySnapshot::create(100, 101, 20, 5);

    ASSERT_FALSE(snapshot.has_value());
    EXPECT_EQ(snapshot.error().code, SnapshotValidationErrorCode::available_memory_exceeds_total);
}

TEST(MemorySnapshot, RejectsFreeSwapAboveTotalSwap) {
    const auto snapshot = MemorySnapshot::create(100, 40, 20, 21);

    ASSERT_FALSE(snapshot.has_value());
    EXPECT_EQ(snapshot.error().code, SnapshotValidationErrorCode::free_swap_exceeds_total);
}

} // namespace
} // namespace memoptima
