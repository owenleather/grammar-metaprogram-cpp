#pragma once

#include "source/grammar.h"

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

struct NodePrinter {
  size_t indent;

  template <typename... T> void operator()(const std::tuple<T...> &tup) {
    std::apply(
        [this](const auto &...element) {
          (NodePrinter{indent + 1}(element), ...);
        },
        tup);
  }

  template <typename T> void operator()(const boost::recursive_wrapper<T> &t) {
    // NOTE (owen): t.get() returns the struct inheriting a Def. If I call
    // .value on the def it gets the original type (i.e. tuple) and works.
    // Otherwise it passes the def directly, and we need to unpack the def
    // later.
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

  template <typename T> void operator()(const Def<T> &d) {
    std::cout << "def" << std::endl;
  }

  void operator()(const auto a) {
    std::cout << "auto: " << typeid(a).name() << '\n';
  }
};

namespace detail {
  
template <typename T> std::string get_pretty_type() {
  const char *mangled_name = typeid(T).name();
#ifndef _MSC_VER
  int status = 0;
  // abi::__cxa_demangle allocates memory that must be freed
  std::unique_ptr<char, void (*)(void *)> demangled(
      abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status), std::free);
  return (status == 0) ? demangled.get() : mangled_name;
#else
  return mangled_name; // MSVC is already human-readable
#endif
}

template <typename T> std::string get_pretty_type_name() {
  int status = -1;
  std::unique_ptr<char, void (*)(void *)> res{
      abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status),
      std::free};
  return (status == 0) ? res.get() : typeid(T).name();
}

}

struct GenericNodePrinter {
  size_t indent;

  void print_indent() const { std::cout << std::string(indent * 2, ' '); }

  void operator()(const detail::DerivedFromTuple auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    using TupleBase = detail::tuple_base_t<DecayedType>;

    print_indent();
    std::cout << detail::get_pretty_type_name<DecayedType>();
    std::cout << ", Tuple:" << std::endl;
    std::apply(
        [this](const auto &...element) {
          (GenericNodePrinter{indent + 1}(element), ...);
        },
        static_cast<const TupleBase &>(t));
  }

  void operator()(const detail::DerivedFromVector auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    using VectorBase = detail::vector_base_t<DecayedType>;

    print_indent();
    std::cout << detail::get_pretty_type_name<DecayedType>();
    std::cout << ", Vector:" << std::endl;
    for (const auto &e : static_cast<const VectorBase &>(t)) {
      GenericNodePrinter{indent + 1}(e);
    }
  }

  void operator()(const detail::DerivedFromVariant auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    using VariantBase = detail::variant_base_t<DecayedType>;

    print_indent();
    std::cout << detail::get_pretty_type_name<DecayedType>();
    std::cout << ", Variant:" << std::endl;
    std::visit(GenericNodePrinter{indent + 1}, static_cast<const VariantBase &>(t));
  }

  void operator()(const detail::DerivedFromWrapper auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    using WrapperBase = detail::wrapper_base_t<DecayedType>;

    GenericNodePrinter{indent}(static_cast<const WrapperBase &>(t).get().value);
  }

  void operator()(const auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    print_indent();
    std::cout << detail::get_pretty_type_name<DecayedType>() << std::endl;
  }
};

} // namespace language