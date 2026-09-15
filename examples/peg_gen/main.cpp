#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "examples/peg_gen/syntax.h"
#include "source/node_printer.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path-to-grammar.peg>\n";
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << argv[1] << "\n";
        return 1;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string peg_grammar = buffer.str();

    language::Context ctx{.input = peg_grammar};

    auto result = language::Matcher<peg::Root>::Match(ctx);

    if (!result) {
        throw std::runtime_error("Parse failed!\n");
    }

    language::GenericNodePrinter{}(result->value.value);
    return 0;
}