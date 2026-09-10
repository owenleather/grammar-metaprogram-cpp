#pragma once
#include "source/grammar.h"
#include <boost/variant/recursive_wrapper.hpp>

namespace example::calculator::syntax {
using namespace language;

template <typename T> using wrap = boost::recursive_wrapper<T>;
template <FixedString T> using r = Regex<T>;

struct Expression;
struct Factor;

using Spacing = r<"[ \\t\\r\\n]*">;
using Float = r<"[0-9]+\\.[0-9]+">;
using Integer = r<"[0-9]+">;

// Number <- (Float / Integer) _
using Number = std::tuple<std::variant<Float, Integer>, Spacing>;

// NOTE (owen): Trying to make these a struct themselves without def makes it so
// the return type is still std::tuple but later variants expect Plus itself.
// This needs to be dealt with somehow -- we need to handle structs implicitly.
// The problem with Def is that Grammar is the GrammarT itself (tuple), but I
// want it to be the struct itself.
struct Plus : std::tuple<r<"\\+">, Spacing> {};
using Minus = std::tuple<r<"\\-">, Spacing>;
using Star = std::tuple<r<"\\*">, Spacing>;
using Slash = std::tuple<r<"\\/">, Spacing>;
using ExponentialOp = std::tuple<r<"\\^">, Spacing>;

using AdditiveOp = std::variant<Plus, Minus>;
using MultiplicativeOp = std::variant<Star, Slash>;

using LParen = std::tuple<r<"\\(">, Spacing>;
using RParen = std::tuple<r<"\\)">, Spacing>;

// Primary <- '(' _ Expression ')' _ / Number
using Primary =
    std::variant<std::tuple<LParen, wrap<Expression>, RParen>, Number>;

// Factor <- Primary ('^' Factor)?
using ExpFactor = std::tuple<ExponentialOp, wrap<Factor>>;
struct Factor : Def<std::tuple<Primary, std::optional<ExpFactor>>> {};

// Term <- Factor (MultiplicativeOp Factor)*
using Term =
    std::tuple<wrap<Factor>,
               std::vector<std::tuple<MultiplicativeOp, wrap<Factor>>>>;

// Expression <- Term (AdditiveOp Term)*
struct Expression
    : Def<std::tuple<Term, std::vector<std::tuple<AdditiveOp, Term>>>> {};

// Calculation <- _ Expression !_
using Calculation = std::tuple<Spacing, wrap<Expression>, Spacing, EndOfFile>;

} // namespace example::calculator::syntax