#include "terminal_text_invariants.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

[[nodiscard]] std::vector<std::filesystem::path>
corpus_files(const std::filesystem::path &directory) {
    std::vector<std::filesystem::path> files;
    for (const auto &entry : std::filesystem::recursive_directory_iterator{directory}) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    return files;
}

} // namespace

int main(const int argument_count, char *arguments[]) {
    if (argument_count != 2) {
        return 2;
    }

    const std::vector<std::filesystem::path> files = corpus_files(arguments[1]);
    if (files.empty()) {
        return 2;
    }

    for (const auto &path : files) {
        std::ifstream input{path, std::ios::binary};
        if (!input.is_open()) {
            return 2;
        }
        const std::string text{std::istreambuf_iterator<char>{input},
                               std::istreambuf_iterator<char>{}};
        memoptima::test::verify_terminal_text_invariants(
            reinterpret_cast<const std::uint8_t *>(text.data()), text.size());
    }

    return 0;
}
