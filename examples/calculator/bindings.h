#include "examples/calculator/syntax.h"
#include "source/grammar.h"
#include "source/node_printer.h"
#include <cmath>

namespace example::calculator::bindings {
using namespace example::calculator::syntax;

struct NumberVisitor {
  double operator()(const Float &f) { return std::stod(f.match); }
  double operator()(const Integer &i) { return std::stoi(i.match); }
};

struct OpVisitor {
  double a;
  double b;
  double operator()(const Plus &) { return a + b; }
  double operator()(const Minus &) { return a - b; }
  double operator()(const Star &) { return a * b; }
  double operator()(const Slash &) { return a / b; }
};

template <typename T>
concept ExpressionOrTerm =
    std::is_same_v<T, Expression::Grammar> || std::is_same_v<T, Term>;

struct Bindings {
  double operator()(const Calculation &c) {
    const auto expr_eval = std::get<1>(c);
    return Bindings{}(expr_eval);
  }

  double operator()(const ExpressionOrTerm auto &node) {
    const auto &[left, right_vec] =
        static_cast<detail::tuple_base_t<decltype(node)>>(node);
    double res = Bindings{}(left);
    for (const auto &[op, right] : right_vec) {
      const double val = Bindings{}(right);
      res = std::visit(OpVisitor{res, val}, op);
    }
    return res;
  }

  double operator()(const Factor::Grammar &f) {
    const auto &[left, maybe_exponent] = f;
    double res = Bindings{}(left);
    if (maybe_exponent.has_value()) {
      const auto exponent = Bindings{}(std::get<1>(maybe_exponent.value()));
      res = std::pow(res, exponent);
    }
    return res;
  }

  double operator()(const ExpFactor &e) { return Bindings{}(std::get<1>(e)); }

  double operator()(const Primary &p) { return std::visit(Bindings{}, p); }

  double operator()(
      const std::variant_alternative_t<0, detail::variant_base_t<Primary>> &t) {
    return Bindings{}(std::get<1>(t));
  }

  double operator()(
      const std::variant_alternative_t<1, detail::variant_base_t<Primary>> &n) {
    return std::visit(NumberVisitor{}, std::get<0>(n));
  }

  template <typename T>
  double operator()(const boost::recursive_wrapper<T> &f) {
    return Bindings{}(f.get().value);
  }
};

// TODO (owen): Dont make variants indent, maybe auto unfold rather than
// printing then unfolding tuples and vectors, and add optional support
struct PrintBindings {
  size_t indent;

  void print_indent() const { std::cout << std::string(indent * 2, ' '); }

  void operator()(const Number &num) {
    print_indent();
    std::cout << "Number: " << std::visit(NumberVisitor{}, std::get<0>(num))
              << std::endl;
  }

  void operator()(const Plus &plus) {
    print_indent();
    std::cout << "Plus" << std::endl;
  }

  void operator()(const detail::DerivedFromTuple auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    using TupleBase = detail::tuple_base_t<DecayedType>;

    print_indent();
    std::cout << detail::get_pretty_type_name_no_namespace<DecayedType>();
    std::cout << ", Tuple:" << std::endl;
    std::apply(
        [this](const auto &...element) {
          (PrintBindings{indent + 1}(element), ...);
        },
        static_cast<const TupleBase &>(t));
  }

  void operator()(const detail::DerivedFromVector auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    using VectorBase = detail::vector_base_t<DecayedType>;

    print_indent();
    std::cout << detail::get_pretty_type_name_no_namespace<DecayedType>();
    std::cout << ", Vector:" << std::endl;
    for (const auto &e : static_cast<const VectorBase &>(t)) {
      PrintBindings{indent + 1}(e);
    }
  }

  void operator()(const detail::DerivedFromVariant auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    using VariantBase = detail::variant_base_t<DecayedType>;

    print_indent();
    std::cout << detail::get_pretty_type_name_no_namespace<DecayedType>();
    std::cout << ", Variant:" << std::endl;
    std::visit(PrintBindings{indent + 1}, static_cast<const VariantBase &>(t));
  }

  void operator()(const detail::DerivedFromWrapper auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    using WrapperBase = detail::wrapper_base_t<DecayedType>;

    PrintBindings{indent}(static_cast<const WrapperBase &>(t).get().value);
  }

  void operator()(const auto &t) {
    using DecayedType = std::decay_t<decltype(t)>;
    print_indent();
    std::cout << detail::get_pretty_type_name_no_namespace<DecayedType>()
              << std::endl;
  }
};
} // namespace example::calculator::bindings
