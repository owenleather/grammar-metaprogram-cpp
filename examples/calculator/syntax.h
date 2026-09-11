#pragma once
#include "source/grammar.h"
#include <boost/variant/recursive_wrapper.hpp>

namespace example::calculator::syntax {
using namespace language;

template <typename T> using wrap = boost::recursive_wrapper<T>;
template <FixedString T> using r = Regex<T>;

struct Expression;
struct Factor;

struct Spacing : r<"[ \\t\\r\\n]*">{};
struct Float : r<"[0-9]+\\.[0-9]+">{};
struct Integer : r<"[0-9]+">{};

// Number <- (Float / Integer) _
struct Number : std::tuple<std::variant<Float, Integer>, Spacing>{};

struct Plus : std::tuple<r<"\\+">, Spacing> {};
struct Minus : std::tuple<r<"\\-">, Spacing>{};
struct Star : std::tuple<r<"\\*">, Spacing>{};
struct Slash : std::tuple<r<"\\/">, Spacing>{};
struct ExponentialOp : std::tuple<r<"\\^">, Spacing>{};

struct AdditiveOp : std::variant<Plus, Minus>{};
struct MultiplicativeOp : std::variant<Star, Slash>{};

struct LParen : std::tuple<r<"\\(">, Spacing>{};
struct RParen : std::tuple<r<"\\)">, Spacing>{};

// Primary <- '(' _ Expression ')' _ / Number
struct Primary :
    std::variant<std::tuple<LParen, wrap<Expression>, RParen>, Number>{};

// Factor <- Primary ('^' Factor)?
struct ExpFactor : std::tuple<ExponentialOp, wrap<Factor>>{};
struct Factor : Def<std::tuple<Primary, std::optional<ExpFactor>>> {};

// Term <- Factor (MultiplicativeOp Factor)*
struct Term :
    std::tuple<wrap<Factor>,
               std::vector<std::tuple<MultiplicativeOp, wrap<Factor>>>>{};

// Expression <- Term (AdditiveOp Term)*
struct Expression
    : Def<std::tuple<Term, std::vector<std::tuple<AdditiveOp, Term>>>> {};

// Calculation <- _ Expression !_
struct Calculation : std::tuple<Spacing, wrap<Expression>, Spacing, EndOfFile>{};

} // namespace example::calculator::syntax