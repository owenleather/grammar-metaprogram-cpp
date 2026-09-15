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

template <typename GrammarT> struct Def {
  using Grammar = GrammarT;
  Grammar value;
  Def(Grammar t) : value(t) {};
};

template <typename Rule> struct Matcher;

template <FixedString Pattern> struct Matcher<Regex<Pattern>> {
  using ReturnType = Regex<Pattern>;

  static std::optional<Result<ReturnType>> Match(Context ctx) {
    static const std::regex re{"^(" + std::string(Pattern.value) + ")",
                               std::regex::optimize};
    return MatchRegex<ReturnType>(re, ctx);
  }
};

template <typename... Types> struct Matcher<std::variant<Types...>> {
  using ReturnType = std::variant<Types...>;

  static std::optional<Result<ReturnType>> Match(Context ctx) {
    std::optional<Result<ReturnType>> result;

    bool matched = ([&]() -> bool {
      if (auto res = Matcher<Types>::Match(ctx)) {
        result = Result<ReturnType>{.ctx = res->ctx,
                                    .value = ReturnType(std::move(res->value))};
        return true;
      }
      return false;
    }() || ...);

    if (matched)
      return result;
    return std::nullopt;
  }
};

template <typename... Elements> struct Matcher<std::tuple<Elements...>> {
  using ReturnType = std::tuple<Elements...>;

  static std::optional<Result<ReturnType>> Match(Context ctx) {
    Context current = ctx;
    std::tuple<std::optional<Elements>...> temp_tuple;

    auto match_elements =
        [&]<std::size_t... Is>(std::index_sequence<Is...>) -> bool {
      bool success = true;
      ((success = success && [&]() -> bool {
         using Element = std::tuple_element_t<Is, ReturnType>;
         if (auto res = Matcher<Element>::Match(current)) {
           current = res->ctx;
           std::get<Is>(temp_tuple).emplace(std::move(res->value));
           return true;
         }
         return false;
       }()),
       ...);
      return success;
    };

    if (!match_elements(std::make_index_sequence<sizeof...(Elements)>{})) {
      return std::nullopt;
    }

    ReturnType final_tuple = std::apply(
        [](auto &&...args) { return std::make_tuple(std::move(*args)...); },
        temp_tuple);

    return Result<ReturnType>{.ctx = current, .value = std::move(final_tuple)};
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

template <typename GrammarT> struct Matcher<Def<GrammarT>> {
  using ReturnType = Def<GrammarT>;

  static std::optional<Result<ReturnType>> Match(Context ctx) {
    auto res = Matcher<GrammarT>::Match(ctx);
    if (!res)
      return std::nullopt;

    return Result<ReturnType>{.ctx = res->ctx,
                              .value = Def<GrammarT>{std::move(res->value)}};
  }
};

template <typename Rule> struct Matcher<std::vector<Rule>> {
  using ReturnType = std::vector<Rule>;

  static std::optional<Result<ReturnType>> Match(Context ctx) {
    Context current = ctx;
    ReturnType children;

    while (auto res = Matcher<Rule>::Match(current)) {
      if (res->ctx.input.size() == current.input.size())
        break;
      children.push_back(std::move(res->value));
      current = res->ctx;
    }

    return Result<ReturnType>{.ctx = current, .value = std::move(children)};
  }
};

template <typename Rule> struct Matcher<std::optional<Rule>> {
  using ReturnType = std::optional<Rule>;

  static std::optional<Result<ReturnType>> Match(Context ctx) {
    if (auto res = Matcher<Rule>::Match(ctx)) {
      return Result<ReturnType>{.ctx = res->ctx,
                                .value = ReturnType(std::move(res->value))};
    }
    return Result<ReturnType>{.ctx = ctx, .value = std::nullopt};
  }
};

template <typename Rule> struct Matcher<Not<Rule>> {
  using ReturnType = Not<Rule>;

  static std::optional<Result<ReturnType>> Match(Context ctx) {
    if (auto res = Matcher<Rule>::Match(ctx))
      return std::nullopt;
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

namespace detail {

// NOTE (owen): We extract the base type of a derived class by attempting to
// call a dummy function extract_template_base. If a class is or derives from
// Template<Args...>, this function can be called successfully, and we can use
// decltype on the function output to get the base type.
template <template <typename...> class Template, typename... Args>
Template<Args...> extract_template_base(const Template<Args...> &);

template <template <auto...> class Template, auto... Args>
Template<Args...> extract_template_base(const Template<Args...> &);

template <typename T, template <auto...> class Template>
using base_auto_t =
    decltype(extract_template_base<Template>(std::declval<T>()));

template <typename T, template <typename...> class Template>
using base_t = decltype(extract_template_base<Template>(std::declval<T>()));

template <typename T, template <typename...> class Template>
concept derived_from_type_template = requires { typename base_t<T, Template>; };

template <typename T, template <auto...> class Template>
concept derived_from_auto_template =
    requires { typename base_auto_t<T, Template>; };
  
// clang-format off
template <typename T> using tuple_base_t = base_t<T, std::tuple>;
template <typename T> using vector_base_t = base_t<T, std::vector>;
template <typename T> using variant_base_t = base_t<T, std::variant>;
template <typename T> using optional_base_t = base_t<T, std::optional>;
template <typename T> using wrapper_base_t = base_t<T, boost::recursive_wrapper>;
template <typename T> using regex_base_t = base_auto_t<T, Regex>;

template <typename T> concept derived_from_tuple = derived_from_type_template<T, std::tuple>;
template <typename T> concept derived_from_vector = derived_from_type_template<T, std::vector>;
template <typename T> concept derived_from_variant = derived_from_type_template<T, std::variant>;
template <typename T> concept derived_from_optional = derived_from_type_template<T, std::optional>;
template <typename T> concept derived_from_wrapper = derived_from_type_template<T, boost::recursive_wrapper>;
template <typename T> concept derived_from_regex = derived_from_auto_template<T, Regex>;
// clang-format on

} // namespace detail

template <typename T, typename Base> struct GenericDerivedMatcher {
  using ReturnType = typename Matcher<Base>::ReturnType;

  static std::optional<Result<T>> Match(Context ctx) {
    const auto match = Matcher<Base>::Match(ctx);
    if (!match)
      return std::nullopt;
    return Result{.ctx = match->ctx, .value = T{std::move(match->value)}};
  }
};

template <typename T>
concept NotAlreadySpecialized = !requires { typename Matcher<T>::ReturnType; };

template <typename T, template <typename...> class Template>
concept DeriveAdapterEligible =
    NotAlreadySpecialized<T> &&
    detail::derived_from_type_template<T, Template> &&
    !std::is_same_v<T, detail::base_t<T, Template>>;

template <typename T, template <auto...> class Template>
concept DeriveAutoAdapterEligible =
    NotAlreadySpecialized<T> &&
    detail::derived_from_auto_template<T, Template> &&
    !std::is_same_v<T, detail::base_auto_t<T, Template>>;

template <typename T, template <typename...> class Template>
  requires DeriveAdapterEligible<T, Template>
struct DerivedMatcher : GenericDerivedMatcher<T, detail::base_t<T, Template>> {
};

template <typename T, template <auto...> class Template>
  requires DeriveAutoAdapterEligible<T, Template>
struct DerivedRegexMatcher
    : GenericDerivedMatcher<T, detail::base_auto_t<T, Template>> {};

template <typename T>
  requires DeriveAdapterEligible<T, std::tuple>
struct Matcher<T> : GenericDerivedMatcher<T, detail::base_t<T, std::tuple>> {};
template <typename T>
  requires DeriveAdapterEligible<T, std::variant>
struct Matcher<T> : GenericDerivedMatcher<T, detail::base_t<T, std::variant>> {
};
template <typename T>
  requires DeriveAdapterEligible<T, std::vector>
struct Matcher<T> : GenericDerivedMatcher<T, detail::base_t<T, std::vector>> {};
template <typename T>
  requires DeriveAdapterEligible<T, std::optional>
struct Matcher<T> : GenericDerivedMatcher<T, detail::base_t<T, std::optional>> {
};
template <typename T>
  requires DeriveAdapterEligible<T, boost::recursive_wrapper>
struct Matcher<T>
    : GenericDerivedMatcher<T, detail::base_t<T, boost::recursive_wrapper>> {};
template <typename T>
  requires DeriveAdapterEligible<T, Not>
struct Matcher<T> : GenericDerivedMatcher<T, detail::base_t<T, Not>> {};
template <typename T>
  requires DeriveAdapterEligible<T, Def>
struct Matcher<T> : GenericDerivedMatcher<T, detail::base_t<T, Def>> {};

template <typename T>
  requires DeriveAutoAdapterEligible<T, Regex>
struct Matcher<T> : GenericDerivedMatcher<T, detail::base_auto_t<T, Regex>> {};

} // namespace language