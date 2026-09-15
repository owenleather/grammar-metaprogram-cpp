#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "examples/peg/generated/zig_metaprogram.h"
#include "source/node_printer.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        throw std::runtime_error("Usage: " + std::string(argv[0]) + " <path-to-grammar.peg>");
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        throw std::runtime_error("Error: Could not open file " + std::string(argv[1]));
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string src_code = buffer.str();

    language::Context ctx{.input = src_code};
    auto result = language::Matcher<zig_metaprogram::Root>::Match(ctx);

    if (!result) {
        throw std::runtime_error("Parse failed!\n");
    }

    language::GenericNodePrinter{}(result->value.value);
    return 0;
}