#pragma once

#include "examples/peg/syntax.h"
#include "source/grammar.h"
// abi::__cxa_demangle is only available on GCC and Clang
#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#define HAS_CXXABI 1
#else
#define HAS_CXXABI 0
#endif

namespace language {

void PrintIndent(const size_t &indent) {
  for (size_t i = 0; i < indent * 2; i++) {
    std::cout << " ";
  };
}

bool IsWhitespaceOnly(const std::string &str) {
  return std::all_of(str.begin(), str.end(),
                     [](unsigned char ch) { return std::isspace(ch); });
}

std::string ResolveIdentifier(const peg::Identifier &id) {
  std::string identifier = std::get<0>(id).match;
  const auto id_vec = std::get<1>(id);
  for (const auto &i : id_vec) {
    identifier += std::visit([](const auto &r) { return r.match; }, i);
  }
  return identifier;
}

struct NodePrinter {
  size_t indent;

  template <typename K>
    requires std::derived_from<K, peg::BaseKeyword>
  void operator()(const K &kw) {
    PrintIndent(indent);
    std::cout << "Keyword: " << typeid(kw).name() << std::endl;
  }

  void operator()(const peg::Identifier &id) {
    PrintIndent(indent);
    std::cout << "Identifier(\"" + ResolveIdentifier(id) + "\")" << std::endl;
  }

  template <typename... T> void operator()(const std::tuple<T...> &tup) {
    std::apply(
        [this](const auto &...element) {
          (NodePrinter{indent + 1}(element), ...);
        },
        tup);
  }

  template <typename T> void operator()(const boost::recursive_wrapper<T> &t) {
    auto value = t.get().value;
    NodePrinter{indent}(value);
  }

  template <typename... T> void operator()(const std::variant<T...> &v) {
    std::visit(NodePrinter{indent}, v);
  }

  template <typename T> void operator()(const std::vector<T> &v) {
    for (const auto &e : v) {
      NodePrinter{indent + 1}(e);
    }
  }

  template <FixedString Str> void operator()(const Regex<Str> &r) {
    if (IsWhitespaceOnly(r.match)) {
      return;
    }

    PrintIndent(indent);
    std::cout << "Regex(\"" << r.match << "\")" << std::endl;
  }

  template <typename T> void operator()(const std::optional<T> &opt) {
    if (opt.has_value()) {
      NodePrinter{indent}(opt.value());
    }
  }
  void operator()(const std::monostate &m) {}
  void operator()(const EndOfFile &m) {}
  template <typename T> void operator()(const And<T> &a) {}
  template <typename T> void operator()(const Not<T> &a) {}
};

} // namespace language