#pragma once

#include "examples/peg/codegen/peg_metaprogram.h"
#include "source/grammar.h"

namespace codegen {
using namespace peg_metaprogram;

std::string ResolveIdentifier(const Identifier &id);

std::string ParseLiteral(const Literal &a);

std::string ParseRange(const Range &a);

std::string ParseExpression(const ExpressionDef::Grammar &expression);

std::string ParseExpression(
    const boost::recursive_wrapper<ExpressionDef> &expression_wrapped);

std::string ParseClass(const Class &cls);

std::string ParsePrimary(const Primary &primary);

std::string ParseSuffix(const Suffix &suffix);

std::string ParsePrefix(const Prefix &prefix);

std::string ParseSequence(const Sequence &sequence);

struct DefinitionResults {
  std::string identifier;
  std::string source_code;
};

DefinitionResults ParseDefinition(const Definition &definition);

std::vector<DefinitionResults> ParseDefinitions(const std::vector<Definition> &definitions);

std::string ParseGrammar(const Grammar &g);

} // namespace language