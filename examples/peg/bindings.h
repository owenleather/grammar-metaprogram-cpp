#pragma once

#include "examples/peg/syntax.h"
#include "source/grammar.h"

namespace language {
using namespace peg;

std::string ResolveIdentifier(const peg::Identifier &id) {
  std::string identifier = std::get<0>(id).match;
  const auto id_vec = std::get<1>(id);
  for (const auto &i : id_vec) {
    identifier += std::visit([](const auto &r) { return r.match; }, i);
  }
  return identifier;
}

// TODO (owen): Make explicit functions like ParseGrammar(), ParseDefinition()
// so the flow is easier to follow. Visitors should be explicitly defined.
struct Bindings {

  std::string operator()(const Grammar &grammar) {
    return Bindings{}(std::get<1>(grammar));
  }

  std::string operator()(const std::vector<Definition> &definitions) {
    std::string out;
    for (const auto &d : definitions) {
      out += Bindings{}(d);
    }
    return out;
  }

  std::string operator()(const Definition &definition) {
    const auto identifier = ResolveIdentifier(std::get<0>(definition));
    const auto expression = Bindings{}(std::get<2>(definition));
    return identifier + " <- " + expression + "\n";
  }

  std::string operator()(const wrap<ExpressionDef> &expression_wrapped) {
    return Bindings{}(expression_wrapped.get().value);
  }

  std::string operator()(const ExpressionDef::Grammar &expression) {
    if (std::get<1>(expression).empty()) {
      return Bindings{}(std::get<0>(expression));
    }

    std::string out = "std::variant<" + Bindings{}(std::get<0>(expression));
    for (const auto &e : std::get<1>(expression)) {
      out += ", " + Bindings{}(std::get<1>(e));
    }
    return out + ">";
  }

  std::string operator()(const Sequence &sequence) {
    std::string out = "std::tuple<";
    for (const auto &p : sequence) {
      out += Bindings{}(p) + ", ";
    }
    out.erase(out.size() - 2, 2);
    return out + ">";
  }

  std::string operator()(const Prefix &prefix) {
    const auto prefix_opt = std::get<0>(prefix);
    if (!prefix_opt.has_value()) {
      return Bindings{}(std::get<1>(prefix));
    }

    if (std::holds_alternative<AND>(prefix_opt.value())) {
      return "And<" + Bindings{}(std::get<1>(prefix)) + ">";
    }
    if (std::holds_alternative<NOT>(prefix_opt.value())) {
      return "Not<" + Bindings{}(std::get<1>(prefix)) + ">";
    }

    throw std::runtime_error("Expected to have AND or NOT in the variant");
  }

  std::string operator()(const std::optional<std::variant<AND, NOT>> &opt) {
    if (!opt.has_value()) {
      return "";
    }
    struct Visitor {
      std::string operator()(const AND &) { return "&"; }
      std::string operator()(const NOT &) { return "!"; }
    };
    return std::visit(Visitor{}, opt.value());
  }

  std::string operator()(const Suffix &suffix) {
    const auto suffix_opt = std::get<1>(suffix);
    if (!suffix_opt.has_value()) {
      return Bindings{}(std::get<0>(suffix));
    }

    if (std::holds_alternative<QUESTION>(suffix_opt.value())) {
      return "std::optional<" + Bindings{}(std::get<0>(suffix)) + ">";
    }
    if (std::holds_alternative<STAR>(suffix_opt.value())) {
      return "std::vector<" + Bindings{}(std::get<0>(suffix)) + ">";
    }
    // TODO (owen): Make this one or more
    if (std::holds_alternative<PLUS>(suffix_opt.value())) {
      return "std::vector<" + Bindings{}(std::get<0>(suffix)) + ">";
    }
    throw std::runtime_error(
        "Expected to have QUESTION, STAR, or PLUS in the variant");
  }

  using PrimaryVariantType = detail::variant_base_t<Primary>;
  std::string operator()(const Primary &primary) {
    const PrimaryVariantType variant_t =
        static_cast<PrimaryVariantType>(primary);
    return std::visit(Bindings{}, variant_t);
  }

  std::string
  operator()(const std::variant_alternative_t<0, PrimaryVariantType> &a) {
    return ResolveIdentifier(std::get<0>(a));
  }

  std::string
  operator()(const std::variant_alternative_t<1, PrimaryVariantType> &a) {
    std::string out = Bindings{}(std::get<1>(a));
    return out;
  }

  using CharVariantType = detail::variant_base_t<Char>;
  struct CharVisitor {
    std::string
    operator()(const std::variant_alternative_t<3, CharVariantType> &c) {
      return std::get<1>(c).match;
    }
    std::string operator()(const auto &t) { return t.match; }
  };

  struct LiteralVisitor {
    std::string operator()(const auto &t) {
      const auto vec = std::get<1>(t);
      std::string out = "r<\"";
      for (const auto &element : vec) {
        out += std::visit(CharVisitor{}, std::get<1>(element));
      }
      return out + "\">";
    }
  };

  std::string operator()(const Literal &a) {
    return std::visit(LiteralVisitor{}, a);
  }

  std::string operator()(const Class &a) {
    std::string out = "r<\"";
    for (const auto &e : std::get<1>(a)) {
      out += Bindings{}(std::get<1>(e));
    }
    out += "\">";
    return out;
  }

  using RangeVariantType = detail::variant_base_t<Range>;
  std::string operator()(const Range &a) {
    struct Visitor {
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

    return std::visit(Visitor{}, a);
  }

  std::string operator()(const DOT &dot) { return "r<\".\">"; }

  std::string operator()(const auto &t) { return "auto"; }
};

} // namespace language