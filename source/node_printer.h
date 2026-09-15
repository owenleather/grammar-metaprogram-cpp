#pragma once

#include "source/grammar.h"
#include <iostream>

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#define HAS_CXXABI 1
#else
#define HAS_CXXABI 0
#endif


namespace language {

namespace detail {

inline std::string strip_namespaces(std::string_view name) {
    static const std::regex ns_regex(R"(\b[a-zA-Z_][a-zA-Z0-9_]*::)");
    return std::regex_replace(std::string(name), ns_regex, "");
}

template <typename T> std::string get_pretty_type() {
  const char *mangled_name = typeid(T).name();
#ifndef _MSC_VER
  int status = 0;
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

template <typename T> std::string get_pretty_type_name_no_namespace() {
  return strip_namespaces(get_pretty_type_name<T>());
}

}

struct GenericNodePrinter {
  size_t indent;

  void print_indent() const { std::cout << std::string(indent * 2, ' '); }

  void operator()(const detail::derived_from_tuple auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    using TupleBase = detail::tuple_base_t<DecayedType>;

    print_indent();
    std::cout << detail::get_pretty_type_name_no_namespace<DecayedType>();
    std::cout << ", Tuple:" << std::endl;
    std::apply(
        [this](const auto &...element) {
          (GenericNodePrinter{indent + 1}(element), ...);
        },
        static_cast<const TupleBase &>(t));
  }

  void operator()(const detail::derived_from_vector auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    using VectorBase = detail::vector_base_t<DecayedType>;

    print_indent();
    std::cout << detail::get_pretty_type_name_no_namespace<DecayedType>();
    std::cout << ", Vector:" << std::endl;
    for (const auto &e : static_cast<const VectorBase &>(t)) {
      GenericNodePrinter{indent + 1}(e);
    }
  }

  void operator()(const detail::derived_from_variant auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    using VariantBase = detail::variant_base_t<DecayedType>;

    std::visit(GenericNodePrinter{indent + 1}, static_cast<const VariantBase &>(t));
  }

  void operator()(const detail::derived_from_wrapper auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    using WrapperBase = detail::wrapper_base_t<DecayedType>;

    GenericNodePrinter{indent}(static_cast<const WrapperBase &>(t).get().value);
  }

  void operator()(const auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    print_indent();
    std::cout << "auto: " << detail::get_pretty_type_name_no_namespace<DecayedType>() << std::endl;
  }
};

} // namespace language