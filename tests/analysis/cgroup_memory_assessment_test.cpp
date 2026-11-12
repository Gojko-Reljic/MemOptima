#include <memoptima/analysis/cgroup_memory_assessment.hpp>

#include <gtest/gtest.h>

#include <expected>
#include <optional>
#include <utility>

namespace memoptima::analysis {
namespace {

[[nodiscard]] CgroupMemorySnapshot
make_snapshot(CgroupByteMetric current, CgroupMemoryLimitMetric high,
              CgroupMemoryLimitMetric maximum, CgroupEventMetric high_events = CgroupEventMetric{0},
              CgroupEventMetric maximum_events = CgroupEventMetric{0},
              CgroupEventMetric oom_events = CgroupEventMetric{0},
              CgroupEventMetric oom_kill_events = CgroupEventMetric{0}) {
    return CgroupMemorySnapshot::create(
        std::move(current), {.high = std::move(high), .maximum = std::move(maximum)},
        {.high = std::move(high_events),
         .maximum = std::move(maximum_events),
         .oom = std::move(oom_events),
         .oom_kill = std::move(oom_kill_events)},
        {.anonymous = CgroupByteMetric{0}, .file_backed = CgroupByteMetric{0}}, CgroupByteMetric{0},
        {.some = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{}},
         .full = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{}}});
}

TEST(CgroupMemoryAssessment, ReportsLowRiskWithoutBoundaryOrOomSignals) {
    const CgroupMemoryAssessment assessment =
        assess_cgroup_memory_snapshot(make_snapshot(89, 95, 100));

    EXPECT_EQ(assessment.overall_risk(), CgroupMemoryRiskLevel::Low);
    EXPECT_EQ(assessment.coverage(), CgroupMemoryAssessmentCoverage::Complete);
    EXPECT_TRUE(assessment.findings().empty());
}

TEST(CgroupMemoryAssessment, AppliesExactMaximumUtilizationBoundaries) {
    const CgroupMemoryAssessment moderate =
        assess_cgroup_memory_snapshot(make_snapshot(90, std::nullopt, 100));
    const CgroupMemoryAssessment high =
        assess_cgroup_memory_snapshot(make_snapshot(98, std::nullopt, 100));
    const CgroupMemoryAssessment at_maximum =
        assess_cgroup_memory_snapshot(make_snapshot(100, std::nullopt, 100));
    const CgroupMemoryAssessment exceeded =
        assess_cgroup_memory_snapshot(make_snapshot(101, std::nullopt, 100));

    ASSERT_EQ(moderate.findings().size(), 1U);
    EXPECT_EQ(moderate.findings().front().code,
              CgroupMemoryFindingCode::memory_max_utilization_moderate);
    ASSERT_EQ(high.findings().size(), 1U);
    EXPECT_EQ(high.findings().front().code, CgroupMemoryFindingCode::memory_max_utilization_high);
    EXPECT_EQ(at_maximum.overall_risk(), CgroupMemoryRiskLevel::High);
    ASSERT_EQ(exceeded.findings().size(), 1U);
    EXPECT_EQ(exceeded.findings().front().code, CgroupMemoryFindingCode::memory_max_exceeded);
    EXPECT_EQ(exceeded.overall_risk(), CgroupMemoryRiskLevel::Critical);
}

TEST(CgroupMemoryAssessment, AvoidsOverflowWhenCalculatingLargeUtilizationThresholds) {
    constexpr std::uint64_t maximum = UINT64_MAX - 1U;
    const CgroupMemoryAssessment assessment =
        assess_cgroup_memory_snapshot(make_snapshot(maximum, std::nullopt, maximum));

    ASSERT_EQ(assessment.findings().size(), 1U);
    EXPECT_EQ(assessment.findings().front().code,
              CgroupMemoryFindingCode::memory_max_utilization_high);
}

TEST(CgroupMemoryAssessment, ReportsCombinedMemoryHighEvidence) {
    const CgroupMemoryAssessment assessment =
        assess_cgroup_memory_snapshot(make_snapshot(100, 100, 1000, CgroupEventMetric{2}));

    ASSERT_EQ(assessment.findings().size(), 1U);
    const CgroupMemoryFinding &finding = assessment.findings().front();
    EXPECT_EQ(finding.code, CgroupMemoryFindingCode::memory_high_reached);
    EXPECT_EQ(finding.risk, CgroupMemoryRiskLevel::Moderate);
    EXPECT_EQ(finding.evidence.size(), 3U);
}

TEST(CgroupMemoryAssessment, ReportsEachHardLimitAndOomFinding) {
    const CgroupMemoryAssessment assessment = assess_cgroup_memory_snapshot(
        make_snapshot(0, std::nullopt, std::nullopt, CgroupEventMetric{0}, CgroupEventMetric{1},
                      CgroupEventMetric{2}, CgroupEventMetric{3}));

    ASSERT_EQ(assessment.findings().size(), 3U);
    EXPECT_EQ(assessment.overall_risk(), CgroupMemoryRiskLevel::Critical);
    EXPECT_EQ(assessment.findings()[0].code, CgroupMemoryFindingCode::oom_kill_recorded);
    EXPECT_EQ(assessment.findings()[1].code, CgroupMemoryFindingCode::memory_max_event_recorded);
    EXPECT_EQ(assessment.findings()[2].code, CgroupMemoryFindingCode::oom_event_recorded);
}

TEST(CgroupMemoryAssessment, IgnoresUnlimitedLimitsForLimitBasedRules) {
    const CgroupMemoryAssessment assessment =
        assess_cgroup_memory_snapshot(make_snapshot(999, std::nullopt, std::nullopt));

    EXPECT_EQ(assessment.overall_risk(), CgroupMemoryRiskLevel::Low);
    EXPECT_TRUE(assessment.findings().empty());
}

TEST(CgroupMemoryAssessment, PreservesDeduplicatedUnavailableEvidenceAndPartialCoverage) {
    const CgroupMetricReadError error{.code = CgroupMetricReadErrorCode::file_open_failed,
                                      .source_name = "memory.events",
                                      .detail = "Unable to open memory.events."};
    const CgroupMemoryAssessment assessment = assess_cgroup_memory_snapshot(
        make_snapshot(0, std::nullopt, std::nullopt, std::unexpected(error), std::unexpected(error),
                      std::unexpected(error), std::unexpected(error)));

    EXPECT_EQ(assessment.coverage(), CgroupMemoryAssessmentCoverage::Partial);
    ASSERT_EQ(assessment.unavailable_evidence().size(), 1U);
    EXPECT_EQ(assessment.unavailable_evidence().front().source_name, "memory.events");
}

} // namespace
} // namespace memoptima::analysis
