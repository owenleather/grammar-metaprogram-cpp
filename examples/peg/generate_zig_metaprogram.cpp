#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "examples/peg/codegen/peg_codegen.h"
#include "examples/peg/codegen/peg_metaprogram.h"

#include "tools/cpp/runfiles/runfiles.h"

using bazel::tools::cpp::runfiles::Runfiles;

const std::string header = R"(
#pragma once

#include "source/grammar.h"

namespace zig_metaprogram {

using namespace language;

template <typename T> using wrap = boost::recursive_wrapper<T>;
template <FixedString T> using r = Regex<T>;
)";

const std::string footer = R"(
} //namespace zig_metaprogram
)";

std::string GetInputFilePath(char *argv[]) {
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::Create(argv[0], &error));

  if (!runfiles) {
    throw std::runtime_error("Failed to load runfiles: " + error);
  }

  return runfiles->Rlocation("_main/examples/peg/data/zig.peg");
}

std::string ReadGrammarFile(char *argv[]) {

  std::string path = GetInputFilePath(argv);
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open runfile at: " + path);
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  const std::string grammar = buffer.str();
  return grammar;
}

std::string GetOutputPath() {
    const char* workspace_dir = std::getenv("BUILD_WORKSPACE_DIRECTORY");
    if (!workspace_dir) {
      throw std::runtime_error("Expected BUILD_WORKSPACE_DIRECTORY environment variable to be set");
    }
    return std::string(workspace_dir) + "/examples/peg/generated/zig_metaprogram.h";
}

int main(int argc, char *argv[]) {
  const auto out_path = GetOutputPath();

  const auto grammar = ReadGrammarFile(argv);
  language::Context ctx{.input = grammar};

  auto result = language::Matcher<peg_metaprogram::Grammar>::Match(ctx);

  if (!result) {
    throw std::runtime_error("Parse failed!\n");
  }

  std::ofstream outfile(out_path);
  if (!outfile.is_open()) {
    throw std::runtime_error("Failed to open file " + out_path);
  }

  outfile << header << std::endl;
  outfile << codegen::ParseGrammar(result->value) << std::endl;
  outfile << footer << std::endl;
  outfile.close();

  return 0;
}