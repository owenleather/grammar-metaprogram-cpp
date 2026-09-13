#pragma once

#include "source/regex_helpers.h"
#include <algorithm>
#include <boost/variant/recursive_wrapper.hpp>
#include <concepts>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace language {

template <FixedString Pattern> struct Regex {
  static constexpr auto pattern = Pattern;
  std::string match;
  Regex(const std::string &match) : match(match) {};
};

template <typename Rule> struct Not {
  using IsNot = void;
};

template <typename Rule> struct And {
  using IsAnd = void;
};

/*
Return Types & Type Deduction Traits
*/

namespace detail {

template <typename T, template <typename...> class Template>
concept DerivedFromTemplate = requires(const T &t) {
  []<typename... Args>(const Template<Args...> &) {}(t);
};

template <typename T>
concept DerivedFromTuple = DerivedFromTemplate<T, std::tuple>;

template <typename T>
concept DerivedFromVariant = DerivedFromTemplate<T, std::variant>;

template <typename T>
concept DerivedFromVector = DerivedFromTemplate<T, std::vector>;

template <typename T>
concept DerivedFromWrapper = DerivedFromTemplate<T, boost::recursive_wrapper>;

template <template <typename...> class Template, typename... Args>
Template<Args...> extract_template_base(const Template<Args...> &);

template <typename T, template <typename...> class Template>
using template_base_t =
    decltype(extract_template_base<Template>(std::declval<T>()));

template <typename T> using tuple_base_t = template_base_t<T, std::tuple>;
template <typename T> using variant_base_t = template_base_t<T, std::variant>;
template <typename T> using vector_base_t = template_base_t<T, std::vector>;
template <typename T> using wrapper_base_t = template_base_t<T, boost::recursive_wrapper>;
template <typename T> using not_base_t = template_base_t<T, Not>;
template <typename T> using and_base_t = template_base_t<T, And>;
template <typename T> using optional_base_t = template_base_t<T, std::optional>;

template <template <FixedString> class Template, FixedString Str>
Template<Str> extract_regex_base(const Template<Str> &);

template <typename T, template<FixedString> class Template>
using regex_base_t = decltype(extract_regex_base<Template>(std::declval<T>()));

} // namespace detail


/*
Recursive Definition
*/
template <typename GrammarT> struct Def {
  using Grammar = GrammarT;
  Grammar value;
  Def(Grammar t) : value(t) {};
};

/*
Matchers
*/

template <typename Rule> struct Matcher;

template <FixedString Pattern> struct Matcher<Regex<Pattern>> {
  using RegexType = Regex<Pattern>;

  static std::optional<Result<RegexType>> Match(Context ctx) {
    static const std::regex re{"^(" + std::string(Pattern.value) + ")",
                               std::regex::optimize};
    return MatchRegex<RegexType>(re, ctx);
  }
};

template <typename T>
  requires(!requires { typename Matcher<T>::RegexType; }) &&
           requires { typename detail::regex_base_t<T, Regex>; }
struct Matcher<T> {
  using BaseRegex = typename detail::regex_base_t<T, Regex>;
  using RegexType = typename Matcher<BaseRegex>::RegexType;

  static std::optional<Result<T>> Match(Context ctx) {
    const auto match = Matcher<BaseRegex>::Match(ctx);
    if (!match) return std::nullopt;

    return Result{.ctx = match->ctx, .value = T{std::move(match->value)}};
  }
};

template <typename... Types>
struct Matcher<std::variant<Types...>> {
  using VariantType = std::variant<Types...>;

  static std::optional<Result<VariantType>> Match(Context ctx) {
    std::optional<Result<VariantType>> result;

    bool matched = ([&]() -> bool {
      if (auto res = Matcher<Types>::Match(ctx)) {
        result = Result<VariantType>{.ctx = res->ctx,
                                     .value = VariantType(std::move(res->value))};
        return true;
      }
      return false;
    }() || ...);

    if (matched) return result;
    return std::nullopt;
  }
};

// Derived Variant Wrapper
template <typename T>
  requires(!requires { typename Matcher<T>::VariantType; }) &&
           requires { typename detail::variant_base_t<T>; }
struct Matcher<T> {
  using BaseVariant = typename detail::variant_base_t<T>;
  using VariantType = typename Matcher<BaseVariant>::VariantType;

  static std::optional<Result<T>> Match(Context ctx) {
    const auto match = Matcher<BaseVariant>::Match(ctx);
    if (!match) return std::nullopt;

    return Result{.ctx = match->ctx, .value = T{std::move(match->value)}};
  }
};

template <typename... Elements>
struct Matcher<std::tuple<Elements...>> {
  using TupleType = std::tuple<Elements...>;

  static std::optional<Result<TupleType>> Match(Context ctx) {
    Context current = ctx;
    std::tuple<std::optional<Elements>...> temp_tuple;

    auto match_elements = [&]<std::size_t... Is>(std::index_sequence<Is...>) -> bool {
      bool success = true;
      ((success = success && [&]() -> bool {
        using Element = std::tuple_element_t<Is, TupleType>;
        if (auto res = Matcher<Element>::Match(current)) {
          current = res->ctx;
          std::get<Is>(temp_tuple).emplace(std::move(res->value));
          return true;
        }
        return false;
      }()), ...);
      return success;
    };

    if (!match_elements(std::make_index_sequence<sizeof...(Elements)>{})) {
      return std::nullopt;
    }

    TupleType final_tuple = std::apply(
        [](auto&&... args) { return std::make_tuple(std::move(*args)...); },
        temp_tuple);

    return Result<TupleType>{.ctx = current, .value = std::move(final_tuple)};
  }
};

// Derived Tuple Wrapper
template <typename T>
  requires(!requires { typename Matcher<T>::TupleType; }) &&
           requires { typename detail::tuple_base_t<T>; }
struct Matcher<T> {
  using BaseTuple = typename detail::tuple_base_t<T>;
  using TupleType = typename Matcher<BaseTuple>::TupleType;

  static std::optional<Result<T>> Match(Context ctx) {
    const auto match = Matcher<BaseTuple>::Match(ctx);
    if (!match) return std::nullopt;

    return Result{.ctx = match->ctx, .value = T{std::move(match->value)}};
  }
};

// 4. Boost Recursive Wrapper
template <typename Target> struct Matcher<boost::recursive_wrapper<Target>> {
  using ReturnType = boost::recursive_wrapper<Target>;

  static std::optional<Result<ReturnType>> Match(Context ctx) {
    auto res = Matcher<typename Target::Grammar>::Match(ctx);
    if (!res) return std::nullopt;

    return Result<ReturnType>{
        .ctx = res->ctx, .value = ReturnType(Target{std::move(res->value)})};
  }
};

// 5. Def<T> Matcher
template <typename GrammarT> struct Matcher<Def<GrammarT>> {
  using ReturnType = Def<GrammarT>;

  static std::optional<Result<ReturnType>> Match(Context ctx) {
    auto res = Matcher<GrammarT>::Match(ctx);
    if (!res) return std::nullopt;

    return Result<ReturnType>{.ctx = res->ctx,
                              .value = Def<GrammarT>{std::move(res->value)}};
  }
};

// 6. Vector Matcher
template <typename Rule> struct Matcher<std::vector<Rule>> {
  using VecType = std::vector<Rule>;

  static std::optional<Result<VecType>> Match(Context ctx) {
    Context current = ctx;
    VecType children;

    while (auto res = Matcher<Rule>::Match(current)) {
      if (res->ctx.input.size() == current.input.size()) break;
      children.push_back(std::move(res->value));
      current = res->ctx;
    }

    return Result<VecType>{.ctx = current, .value = std::move(children)};
  }
};

// Derived Vector Wrapper
template <typename T>
  requires(!requires { typename Matcher<T>::VecType; }) &&
           requires { typename detail::vector_base_t<T>; }
struct Matcher<T> {
  using BaseVec = typename detail::vector_base_t<T>;
  using VecType = typename Matcher<BaseVec>::VecType;

  static std::optional<Result<T>> Match(Context ctx) {
    const auto match = Matcher<BaseVec>::Match(ctx);
    if (!match) return std::nullopt;

    return Result{.ctx = match->ctx, .value = T{std::move(match->value)}};
  }
};

// 7. Optional, Not, And
template <typename Rule> struct Matcher<std::optional<Rule>> {
  using OptType = std::optional<Rule>;

  static std::optional<Result<OptType>> Match(Context ctx) {
    if (auto res = Matcher<Rule>::Match(ctx)) {
      return Result<OptType>{.ctx = res->ctx,
                             .value = OptType(std::move(res->value))};
    }
    return Result<OptType>{.ctx = ctx, .value = std::nullopt};
  }
};

template <typename T>
  requires(!requires { typename Matcher<T>::OptType; }) &&
           requires { typename detail::optional_base_t<T>; }
struct Matcher<T> {
  using BaseOptional = typename detail::not_base_t<T>;
  using OptType = typename Matcher<BaseOptional>::OptType;

  static std::optional<Result<T>> Match(Context ctx) {
    const auto match = Matcher<BaseOptional>::Match(ctx);
    if (!match) return std::nullopt;

    return Result{.ctx = match->ctx, .value = T{std::move(match->value)}};
  }
};

template <typename Rule> struct Matcher<Not<Rule>> {
  using NotType = Not<Rule>;

  static std::optional<Result<NotType>> Match(Context ctx) {
    if (auto res = Matcher<Rule>::Match(ctx)) return std::nullopt;
    return Result{.ctx = ctx, .value = Not<Rule>{}};
  }
};


template <typename T>
  requires(!requires { typename Matcher<T>::NotType; }) &&
           requires { typename detail::not_base_t<T>; }
struct Matcher<T> {
  using BaseNot = typename detail::not_base_t<T>;
  using NotType = typename Matcher<BaseNot>::NotType;

  static std::optional<Result<T>> Match(Context ctx) {
    const auto match = Matcher<BaseNot>::Match(ctx);
    if (!match) return std::nullopt;

    return Result{.ctx = match->ctx, .value = T{std::move(match->value)}};
  }
};

template <typename Rule> struct Matcher<And<Rule>> {
  using AndType = And<Rule>;

  static std::optional<Result<AndType>> Match(Context ctx) {
    if (auto res = Matcher<Rule>::Match(ctx)) {
      return Result{.ctx = ctx, .value = And<Rule>{}};
    }
    return std::nullopt;
  }
};

template <typename T>
  requires(!requires { typename Matcher<T>::AndType; }) &&
           requires { typename detail::and_base_t<T>; }
struct Matcher<T> {
  using BaseAnd = typename detail::and_base_t<T>;
  using AndType = typename Matcher<BaseAnd>::AndType;

  static std::optional<Result<T>> Match(Context ctx) {
    const auto match = Matcher<BaseAnd>::Match(ctx);
    if (!match) return std::nullopt;

    return Result{.ctx = match->ctx, .value = T{std::move(match->value)}};
  }
};


} // namespace language