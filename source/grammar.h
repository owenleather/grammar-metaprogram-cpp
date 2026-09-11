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
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace language {

/*
Rule Tags
*/

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

struct EndOfFile {
  using IsEOF = void;
};

/*
Return Types
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
    if (!match)
      return std::nullopt;

    T struct_val{std::move(match->value)};

    return Result{.ctx = match->ctx, .value = std::move(struct_val)};
  }
};

template <typename Head> struct Matcher<std::variant<Head>> {
  using VariantType = std::variant<Head>;

  static std::optional<Result<VariantType>> Match(Context ctx) {
    if (auto res = Matcher<Head>::Match(ctx)) {
      return Result<VariantType>{.ctx = res->ctx,
                                 .value = VariantType(std::move(res->value))};
    }
    return std::nullopt;
  }
};

template <typename Head, typename... Tail>
struct Matcher<std::variant<Head, Tail...>> {
  using VariantType = std::variant<Head, Tail...>;

  static std::optional<Result<VariantType>> Match(Context ctx) {
    auto head = Matcher<Head>::Match(ctx);
    if (head) {
      return Result<VariantType>{.ctx = head->ctx,
                                 .value = VariantType(std::move(head->value))};
    }

    if constexpr (sizeof...(Tail) > 0) {
      auto tail = Matcher<std::variant<Tail...>>::Match(ctx);
      if (tail) {
        VariantType parent_variant = std::visit(
            [](auto &&val) -> VariantType {
              return VariantType(std::forward<decltype(val)>(val));
            },
            tail->value);

        return Result<VariantType>{.ctx = tail->ctx,
                                   .value = std::move(parent_variant)};
      }
    }

    return std::nullopt;
  }
};

// TODO (owen): Figure out if we need a derived specialization for the singular
// Head case
template <typename T>
  requires(!requires { typename Matcher<T>::VariantType; }) &&
          requires { typename detail::variant_base_t<T>; }
struct Matcher<T> {
  using BaseVariant = typename detail::variant_base_t<T>;
  using VariantType = typename Matcher<BaseVariant>::VariantType;

  static std::optional<Result<T>> Match(Context ctx) {
    const auto match = Matcher<BaseVariant>::Match(ctx);
    if (!match)
      return std::nullopt;

    T struct_val{std::move(match->value)};

    return Result{.ctx = match->ctx, .value = std::move(struct_val)};
  }
};

template <typename Head, typename... Tail>
struct Matcher<std::tuple<Head, Tail...>> {
  using TupleType = std::tuple<Head, Tail...>;

  static std::optional<Result<TupleType>> Match(Context ctx) {
    auto head_res = Matcher<Head>::Match(ctx);
    if (!head_res)
      return std::nullopt;

    if constexpr (sizeof...(Tail) == 0) {
      return Result<TupleType>{.ctx = head_res->ctx,
                               .value =
                                   std::make_tuple(std::move(head_res->value))};
    } else {
      auto tail_res = Matcher<std::tuple<Tail...>>::Match(head_res->ctx);
      if (!tail_res)
        return std::nullopt;

      return Result<TupleType>{
          .ctx = tail_res->ctx,
          .value = std::tuple_cat(std::make_tuple(std::move(head_res->value)),
                                  std::move(tail_res->value))};
    }
  }
};

template <typename T>
  requires(!requires { typename Matcher<T>::TupleType; }) &&
          requires { typename detail::tuple_base_t<T>; }
struct Matcher<T> {
  using BaseTuple = typename detail::tuple_base_t<T>;
  using TupleType = typename Matcher<BaseTuple>::TupleType;

  static std::optional<Result<T>> Match(Context ctx) {
    const auto match = Matcher<BaseTuple>::Match(ctx);
    if (!match)
      return std::nullopt;

    T struct_val{std::move(match->value)};

    return Result{.ctx = match->ctx, .value = std::move(struct_val)};
  }
};

template <typename Target> struct Matcher<boost::recursive_wrapper<Target>> {
  using ReturnType = boost::recursive_wrapper<Target>;

  static std::optional<Result<ReturnType>> Match(Context ctx) {
    auto res = Matcher<typename Target::Grammar>::Match(ctx);
    if (!res)
      return std::nullopt;

    return Result<ReturnType>{
        .ctx = res->ctx, .value = ReturnType(Target{std::move(res->value)})};
  }
};

template <typename Rule> struct Matcher<std::vector<Rule>> {
  using VecType = std::vector<Rule>;

  static std::optional<Result<VecType>> Match(Context ctx) {
    Context current = ctx;
    VecType children;

    while (auto res = Matcher<Rule>::Match(current)) {
      if (res->ctx.input.size() == current.input.size())
        break;
      children.push_back(std::move(res->value));
      current = res->ctx;
    }

    return Result<VecType>{.ctx = current, .value = std::move(children)};
  }
};

template <typename T>
  requires(!requires { typename Matcher<T>::VecType; }) &&
          requires { typename detail::vector_base_t<T>; }
struct Matcher<T> {
  using BaseVec = typename detail::vector_base_t<T>;
  using VecType = typename Matcher<BaseVec>::VecType;

  static std::optional<Result<T>> Match(Context ctx) {
    const auto match = Matcher<BaseVec>::Match(ctx);
    if (!match)
      return std::nullopt;

    T struct_val{std::move(match->value)};

    return Result{.ctx = match->ctx, .value = std::move(struct_val)};
  }
};

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

template <typename Rule> struct Matcher<Not<Rule>> {
  using ReturnType = Not<Rule>;

  static std::optional<Result<ReturnType>> Match(Context ctx) {
    if (auto res = Matcher<Rule>::Match(ctx)) {
      return std::nullopt;
    }
    return Result{.ctx = ctx, .value = Not<Rule>{}};
  }
};

template <typename Rule> struct Matcher<And<Rule>> {
  using ReturnType = And<Rule>;

  static std::optional<Result<ReturnType>> Match(Context ctx) {
    if (auto res = Matcher<Rule>::Match(ctx)) {
      return Result{.ctx = ctx, .value = And<Rule>{}};
    }
    return std::nullopt;
  }
};

template <> struct Matcher<EndOfFile> {
  static std::optional<Result<EndOfFile>> Match(Context ctx) {
    if (ctx.input.empty()) {
      return Result<EndOfFile>{.ctx = ctx, .value = EndOfFile{}};
    }
    return std::nullopt;
  }
};

} // namespace language