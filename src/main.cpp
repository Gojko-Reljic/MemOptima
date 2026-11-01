#include <memoptima/application_info.hpp>

#include <iostream>
#include <string_view>

namespace {

constexpr int kExitSuccess = 0;
constexpr int kExitUsageError = 2;

void print_help(std::ostream& output) {
    output << memoptima::application_name() << " — " << memoptima::application_description() << "\n\n"
           << "Usage:\n"
           << "  memoptima [OPTION]\n\n"
           << "Options:\n"
           << "  -h, --help       Display this help message.\n"
           << "  -v, --version    Display the application version.\n";
}

int run(int argument_count, char* arguments[]) {
    if (argument_count == 1) {
        print_help(std::cout);
        return kExitSuccess;
    }

    if (argument_count != 2) {
        std::cerr << "Error: expected at most one option.\n\n";
        print_help(std::cerr);
        return kExitUsageError;
    }

    const std::string_view option{arguments[1]};

    if (option == "-h" || option == "--help") {
        print_help(std::cout);
        return kExitSuccess;
    }

    if (option == "-v" || option == "--version") {
        std::cout << "memoptima " << memoptima::version() << '\n';
        return kExitSuccess;
    }

    std::cerr << "Error: unknown option '" << option << "'.\n\n";
    print_help(std::cerr);
    return kExitUsageError;
}

}  // namespace

int main(int argument_count, char* arguments[]) {
    return run(argument_count, arguments);
}
