#include "examples/calculator/syntax.h"
#include "examples/calculator/bindings.h"
#include "source/node_printer.h"
#include <iostream>
#include <cmath>

using namespace example::calculator::syntax;
using namespace example::calculator::bindings;

struct A{};

int main() {
  const std::string input = "4.3 + (7 + 3.4^(5 - 3)) * 6 / 4.2";
  const auto actual_value = 4.3 + (7 + std::pow(3.4,5-3)) * 6 / 4.2;

  const language::Context ctx{.input = input};
  const auto result = Matcher<Calculation>::Match(ctx);
  
  if (!result) {
    throw std::runtime_error("Parse failed!\n");
  }
  
  GenericNodePrinter{}(result->value);
  
  const auto parsed_value = Bindings{}(result->value);
  std::cout << "Parsed expression value: " << parsed_value << std::endl;
  std::cout << "Actual value: " << actual_value << std::endl;
  
  if (actual_value == parsed_value) {
    std::cout << "Parsed value equals actual value!" << std::endl;
  } else{
    std::cout << "Parsed value does not equal actual value." << std::endl;
  }
}