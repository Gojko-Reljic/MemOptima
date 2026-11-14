#include "cli/application_runner.hpp"

int main(int argument_count, char *arguments[]) {
    return memoptima::cli::run_application(argument_count, arguments);
}
