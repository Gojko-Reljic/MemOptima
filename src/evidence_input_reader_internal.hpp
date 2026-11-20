#pragma once

#include <memoptima/evidence_input_reader.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>

namespace memoptima::detail {

// Opens a relative artifact by walking from an already opened root directory without following
// symbolic links in any path component. This is intentionally internal to evidence consumers.
[[nodiscard]] std::expected<EvidenceInputReader, EvidenceInputError>
open_evidence_input_beneath(const std::filesystem::path &root,
                            const std::filesystem::path &relative_path,
                            std::uint64_t maximum_line_bytes, std::uint64_t maximum_total_bytes);

} // namespace memoptima::detail
