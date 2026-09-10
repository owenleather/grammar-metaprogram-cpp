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

template <typename T>
struct get_tuple_base;

// Matches std::tuple directly or via base-class conversion
template <typename... Args>
struct get_tuple_base<std::tuple<Args...>> {
  using type = std::tuple<Args...>;
};

template <typename Derived>
  requires requires(Derived& d) {
    []<typename... Args>(std::tuple<Args...>&){}(d);
  }
struct get_tuple_base<Derived> {
 private:
  template <typename... Args>
  static std::tuple<Args...> extract(const std::tuple<Args...>*);

 public:
  using type = decltype(extract(std::declval<Derived*>()));
};


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
  using ReturnType = Regex<Pattern>;

  static std::optional<Result<ReturnType>> Match(Context ctx) {
    static const std::regex re{"^(" + std::string(Pattern.value) + ")",
                               std::regex::optimize};
    return MatchRegex<ReturnType>(re, ctx);
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
  requires (!requires { typename Matcher<T>::TupleType; }) // avoids re-matching std::tuple itself
        && requires { typename get_tuple_base<T>::type; }
struct Matcher<T> {
  using BaseTuple = typename get_tuple_base<T>::type;
  using TupleType = typename Matcher<BaseTuple>::TupleType;

  static std::optional<Result<T>> Match(Context ctx) {
    const auto match = Matcher<BaseTuple>::Match(ctx);
    if(!match) return std::nullopt;

    T struct_val{std::move(match->value)};

    return Result{
      .ctx = match->ctx,
      .value = std::move(struct_val)
    };
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