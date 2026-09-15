#include "examples/peg_gen/codegen/peg_codegen.h"
#include <iomanip>
#include <iostream>
#include <sstream>

namespace codegen {

std::string ResolveIdentifier(const Identifier &id) {
  std::string identifier = std::get<0>(id).match;
  const auto id_vec = std::get<1>(id);
  for (const auto &i : id_vec) {
    identifier += std::visit([](const auto &r) { return r.match; }, i);
  }
  return identifier;
}

// NOTE (owen): We must convert octal strings to a \xHH hex escape sequence
// since std::regex does not support octal strings.
std::string OctalToHex8Bit(const std::string &oct_str) {
  std::string processed_str = oct_str;
  while (processed_str.starts_with("\\")) {
    processed_str.erase(0, 1);
  }

  if (processed_str.empty()) {
    throw std::invalid_argument("Input string is empty.");
  }

  std::size_t processed_chars = 0;
  unsigned long value = std::stoul(processed_str, &processed_chars, 8);

  if (processed_chars != processed_str.length()) {
    throw std::invalid_argument("Input string contains non-octal characters: " +
                                processed_str);
  }

  if (value > UINT8_MAX) {
    throw std::out_of_range(
        "Octal value exceeds 8-bit range: " + processed_str);
  }

  std::ostringstream oss;
  oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
      << value;

  return "\\x" + oss.str();
}

using CharVariantType = detail::variant_base_t<Char>;
struct CharVisitor {
  std::string
  operator()(const std::variant_alternative_t<0, CharVariantType> &chr) {
    const std::string esc_seq = chr.match;
    if (esc_seq == "\\n")
      return "\\n";
    if (esc_seq == "\\r")
      return "\\r";
    if (esc_seq == "\\t")
      return "\\t";
    if (esc_seq == "\\'")
      return "'";
    if (esc_seq == "\\\"")
      return "\"";
    if (esc_seq == "\\[")
      return "\\[";
    if (esc_seq == "\\]")
      return "\\]";
    if (esc_seq == "\\\\")
      return "\\\\";
    return esc_seq;
  }

  std::string
  operator()(const std::variant_alternative_t<1, CharVariantType> &chr) {
    return OctalToHex8Bit(chr.match);
  }

  std::string
  operator()(const std::variant_alternative_t<2, CharVariantType> &chr) {
    return OctalToHex8Bit(chr.match);
  }

  std::string
  operator()(const std::variant_alternative_t<3, CharVariantType> &chr) {
    const std::string char_val = std::get<1>(chr).match;

    if (char_val == ".")
      return "\\.";
    if (char_val == "?")
      return "\\?";
    if (char_val == "*")
      return "\\*";
    if (char_val == "+")
      return "\\+";
    if (char_val == "^")
      return "\\^";
    if (char_val == "$")
      return "\\$";
    if (char_val == "|")
      return "\\|";
    if (char_val == "(")
      return "\\(";
    if (char_val == ")")
      return "\\)";
    if (char_val == "[")
      return "\\[";
    if (char_val == "]")
      return "\\]";
    if (char_val == "{")
      return "\\{";
    if (char_val == "}")
      return "\\}";

    return char_val;
  }
};

struct LiteralVisitor {
  std::string operator()(const auto &t) {
    const auto &vec = std::get<1>(t);
    std::string out = "r<R\"(";
    for (const auto &element : vec) {
      // Extract the Char variant from tuple element
      const auto &char_node = std::get<1>(element);
      const CharVariantType char_v = static_cast<CharVariantType>(char_node);
      out += std::visit(CharVisitor{}, char_v);
    }
    return out + ")\">";
  }
};

std::string ParseLiteral(const Literal &a) {
  return std::visit(LiteralVisitor{}, a);
}

using RangeVariantType = detail::variant_base_t<Range>;
struct RangeVisitor {
  std::string
  operator()(const std::variant_alternative_t<0, RangeVariantType> &r) {
    return std::visit(CharVisitor{}, std::get<0>(r)) + "-" +
           std::visit(CharVisitor{}, std::get<2>(r));
  }
  std::string
  operator()(const std::variant_alternative_t<1, RangeVariantType> &r) {
    return std::visit(CharVisitor{}, r);
  };
};

std::string ParseRange(const Range &a) { return std::visit(RangeVisitor{}, a); }

std::string ParseExpression(const ExpressionDef::Grammar &expression) {
  const auto sequence = std::get<0>(expression);
  const auto parsed_sequence = ParseSequence(sequence);
  const auto sequences = std::get<1>(expression);
  if (sequences.empty()) {
    return parsed_sequence;
  }

  std::string out = "std::variant<" + parsed_sequence;
  for (const auto &e : std::get<1>(expression)) {
    out += ", " + ParseSequence(std::get<1>(e));
  }
  return out + ">";
}

std::string ParseExpression(
    const boost::recursive_wrapper<ExpressionDef> &expression_wrapped) {
  return ParseExpression(expression_wrapped.get().value);
}

std::string ParseClass(const Class &cls) {
  std::string out = "r<R\"([";
  const auto &elements = std::get<1>(cls);
  for (const auto &elem : elements) {
    const auto &range = std::get<1>(elem);
    out += ParseRange(range);
  }
  out += "])\">";
  return out;
}

using PrimaryVariantType = detail::variant_base_t<Primary>;
struct PrimaryVisitor {
  std::string
  operator()(const std::variant_alternative_t<0, PrimaryVariantType> &a) {
    return "wrap<" + ResolveIdentifier(std::get<0>(a)) + ">";
  }

  std::string
  operator()(const std::variant_alternative_t<1, PrimaryVariantType> &a) {
    return ParseExpression(std::get<1>(a));
  }

  std::string operator()(const Literal &literal) {
    return ParseLiteral(literal);
  }

  std::string operator()(const Class &cls) { return ParseClass(cls); }
  std::string operator()(const DOT &dot) { return "r<R\"(.)\">"; }
};

std::string ParsePrimary(const Primary &primary) {
  const PrimaryVariantType variant_t = static_cast<PrimaryVariantType>(primary);
  return std::visit(PrimaryVisitor{}, variant_t);
}

std::string ParseSuffix(const Suffix &suffix) {
  const auto suffix_opt = std::get<1>(suffix);
  const auto primary = std::get<0>(suffix);
  const auto parsed_primary = ParsePrimary(primary);
  if (!suffix_opt.has_value()) {
    return parsed_primary;
  }

  if (std::holds_alternative<QUESTION>(suffix_opt.value())) {
    return "std::optional<" + parsed_primary + ">";
  }
  if (std::holds_alternative<STAR>(suffix_opt.value())) {
    return "std::vector<" + parsed_primary + ">";
  }
  // TODO (owen): Make this one or more
  if (std::holds_alternative<PLUS>(suffix_opt.value())) {
    return "std::vector<" + parsed_primary + ">";
  }

  throw std::runtime_error(
      "Expected to have QUESTION, STAR, or PLUS in the variant");
}

std::string ParsePrefix(const Prefix &prefix) {
  const auto prefix_opt = std::get<0>(prefix);
  const auto suffix = std::get<1>(prefix);
  const auto parsed_suffix = ParseSuffix(suffix);
  if (!prefix_opt.has_value()) {
    return parsed_suffix;
  }

  if (std::holds_alternative<AND>(prefix_opt.value())) {
    return "And<" + parsed_suffix + ">";
  }
  if (std::holds_alternative<NOT>(prefix_opt.value())) {
    return "Not<" + parsed_suffix + ">";
  }

  throw std::runtime_error("Expected to have AND or NOT in the variant");
}

std::string ParseSequence(const Sequence &sequence) {
  if (sequence.size() == 1) {
    return ParsePrefix(sequence.front());
  }
  std::string out = "std::tuple<";
  for (const auto &prefix : sequence) {
    out += ParsePrefix(prefix) + ", ";
  }
  out.erase(out.size() - 2, 2);
  return out + ">";
}

DefinitionResults ParseDefinition(const Definition &definition) {
  const auto identifier = ResolveIdentifier(std::get<0>(definition));
  const auto expression = ParseExpression(std::get<2>(definition));
  return {.identifier = identifier,
          .source_code =
              "struct " + identifier + " : Def<" + expression + ">{}"};
}

std::vector<DefinitionResults>
ParseDefinitions(const std::vector<Definition> &definitions) {
  std::vector<DefinitionResults> out;
  out.reserve(definitions.size());
  for (const auto &d : definitions) {
    out.push_back(ParseDefinition(d));
  }
  return out;
}

std::string ParseGrammar(const Grammar &g) {
  const auto definitions = ParseDefinitions(std::get<1>(g));
  std::string forward_declarations_src;
  std::string definitions_src;
  for (const auto &definition : definitions) {
    forward_declarations_src += "struct " + definition.identifier + ";\n";
    definitions_src += definition.source_code + ";\n";
  }

  return forward_declarations_src + definitions_src;
}

} // namespace codegen