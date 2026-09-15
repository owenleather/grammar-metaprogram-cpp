/*
Figure 1. of https://bford.info/pub/lang/peg.pdf
PEG formally describing its own ASCII syntax

# Hierarchical syntax
Grammar <- Spacing Definition+ EndOfFile
Definition <- Identifier LEFTARROW Expression

Expression <- Sequence (SLASH Sequence)*
Sequence <- Prefix*
Prefix <- (AND / NOT)? Suffix
Suffix <- Primary (QUESTION / STAR / PLUS)?
Primary <- Identifier !LEFTARROW
/ OPEN Expression CLOSE
/ Literal / Class / DOT

# Lexical syntax
Identifier <- IdentStart IdentCont* Spacing
IdentStart <- [a-zA-Z_]
IdentCont <- IdentStart / [0-9]

Literal <- [’] (![’] Char)* [’] Spacing
/ ["] (!["] Char)* ["] Spacing
Class <- ’[’ (!’]’ Range)* ’]’ Spacing
Range <- Char ’-’ Char / Char
Char <- ’\\’ [nrt’"\[\]\\]
/ ’\\’ [0-2][0-7][0-7]
/ ’\\’ [0-7][0-7]?
/ !’\\’ .

LEFTARROW <- ’<-’ Spacing
SLASH <- ’/’ Spacing
AND <- ’&’ Spacing
NOT <- ’!’ Spacing
QUESTION <- ’?’ Spacing
STAR <- ’*’ Spacing
PLUS <- ’+’ Spacing
OPEN <- ’(’ Spacing
CLOSE <- ’)’ Spacing
DOT <- ’.’ Spacing

Spacing <- (Space / Comment)*
Comment <- ’#’ (!EndOfLine .)* EndOfLine
Space <- ’ ’ / ’\t’ / EndOfLine
EndOfLine <- ’\r\n’ / ’\n’ / ’\r’
EndOfFile <- !.
*/

#pragma once

#include "source/grammar.h"

namespace peg_metaprogram {

using namespace language;

template <typename T> using wrap = boost::recursive_wrapper<T>;
template <FixedString T> using r = Regex<T>;

struct ExpressionDef;

// clang-format off
struct EndOfFile : Not<r<R"(.)">>{};
struct EndOfLine : std::variant<r<"\r\n">, r<"\n">, r<"\r">>{};
struct Space     : std::variant<r<" ">, r<"\t">, EndOfLine>{};

struct Comment : std::tuple<
    r<"#">,
    std::vector<std::tuple<Not<EndOfLine>, r<".">>>,
    EndOfLine
>{};

struct Spacing : std::vector<std::variant<Space, Comment>>{};

template <FixedString Str> 
struct Keyword : std::tuple<r<Str>, Spacing>{};

struct LEFTARROW : Keyword<"<-">{};
struct SLASH     : Keyword<"/">{};
struct AND       : Keyword<"&">{};
struct NOT       : Keyword<"!">{};
struct QUESTION  : Keyword<"\\?">{};
struct STAR      : Keyword<"\\*">{};
struct PLUS      : Keyword<"\\+">{};
struct OPEN      : Keyword<"\\(">{};
struct CLOSE     : Keyword<"\\)">{};
struct DOT       : Keyword<"\\.">{};

struct IdentStart : r<"[a-zA-Z_]">{};
struct IdentCont  : std::variant<IdentStart, r<"[0-9]">>{};
struct Identifier : std::tuple<IdentStart, std::vector<IdentCont>, Spacing>{};

// NOTE (owen): Different from Figure 1, the second variant was updated from
// [0-2] to [0-3] to allow full 8-bit Extended ASCII characters instead of
// classical 7-bit ASCII characters.
struct Char : std::variant<
    r<"\\\\([nrt'\"\\[\\]\\\\])">,
    r<"\\\\([0-3][0-7][0-7])">,
    r<"\\\\([0-7]{1,2})">, 
    std::tuple<Not<r<"\\\\">>, r<".">>
>{};

struct Range : std::variant<
    std::tuple<Char, r<"-">, Char>,
    Char
>{};

struct Class : std::tuple<
    r<"\\[">,
    std::vector<std::tuple<Not<r<"\\]">>, Range>>,
    r<"\\]">,
    Spacing
>{};

struct Literal : std::variant<
    std::tuple<r<"'">, std::vector<std::tuple<Not<r<"'">>, Char>>, r<"'">, Spacing>,
    std::tuple<r<"\"">, std::vector<std::tuple<Not<r<"\"">>, Char>>, r<"\"">, Spacing>
>{};

struct Primary : std::variant<
    std::tuple<Identifier, Not<LEFTARROW>>,
    std::tuple<OPEN, wrap<ExpressionDef>, CLOSE>,
    Literal,
    Class,
    DOT
>{};

struct Suffix   : std::tuple<Primary, std::optional<std::variant<QUESTION, STAR, PLUS>>>{};
struct Prefix   : std::tuple<std::optional<std::variant<AND, NOT>>, Suffix>{};
struct Sequence : std::vector<Prefix>{};

struct ExpressionDef : Def<std::tuple<Sequence, std::vector<std::tuple<SLASH, Sequence>>>> {};

struct Definition : std::tuple<Identifier, LEFTARROW, wrap<ExpressionDef>>{};
struct Grammar   : std::tuple<Spacing, std::vector<Definition>, EndOfFile>{};
//clang-format on

} // namespace peg_metaprogram
