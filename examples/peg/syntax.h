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

namespace peg {

using namespace language;

template <typename T> using wrap = boost::recursive_wrapper<T>;
template <FixedString T> using r = Regex<T>;

struct ExpressionDef;

// clang-format off
using EndOfLine = std::variant<r<"\r\n">, r<"\n">, r<"\r">>;
using Space      = std::variant<r<" ">, r<"\t">, EndOfLine>;

using Comment = std::tuple<
    r<"#">,
    std::vector<std::tuple<Not<EndOfLine>, r<".">>>,
    EndOfLine
>;

using Spacing   = std::vector<std::variant<Space, Comment>>;

// TODO (owen): I am trying to make these structs because I would like the ability for types to exist rather than just being aliases. That way they're easier to work with and error messages are less verbose. 
struct LEFTARROW : std::tuple<r<"<-">, Spacing>{};
using SLASH     = std::tuple<r<"/">, Spacing>;
using AND       = std::tuple<r<"&">, Spacing>;
using NOT       = std::tuple<r<"!">, Spacing>;
using QUESTION  = std::tuple<r<"\\?">, Spacing>;
using STAR      = std::tuple<r<"\\*">, Spacing>;
using PLUS      = std::tuple<r<"\\+">, Spacing>;
using OPEN      = std::tuple<r<"\\(">, Spacing>;
using CLOSE     = std::tuple<r<"\\)">, Spacing>;
using DOT       = std::tuple<r<"\\.">, Spacing>;

using IdentStart = r<"[a-zA-Z_]">;
using IdentCont  = std::variant<IdentStart, r<"[0-9]">>;
using Identifier = std::tuple<IdentStart, std::vector<IdentCont>, Spacing>;

using Char = std::variant<
    r<"\\\\([nrt'\"\\[\\]\\\\])">,
    r<"\\\\([0-2][0-7][0-7])">,
    r<"\\\\([0-7]{1,2})">, 
    std::tuple<Not<r<"\\\\">>, r<".">>
>;

using Range = std::variant<
    std::tuple<Char, r<"-">, Char>,
    Char
>;

using Class = std::tuple<
    r<"\\[">,
    std::vector<std::tuple<Not<r<"\\]">>, Range>>,
    r<"\\]">,
    Spacing
>;

using Literal = std::variant<
    std::tuple<r<"'">, std::vector<std::tuple<Not<r<"'">>, Char>>, r<"'">, Spacing>,
    std::tuple<r<"\"">, std::vector<std::tuple<Not<r<"\"">>, Char>>, r<"\"">, Spacing>
>;

using Primary = std::variant<
    std::tuple<Identifier, Not<LEFTARROW>>,
    std::tuple<OPEN, wrap<ExpressionDef>, CLOSE>,
    Literal,
    Class,
    DOT
>;

using Suffix   = std::tuple<Primary, std::optional<std::variant<QUESTION, STAR, PLUS>>>;
using Prefix   = std::tuple<std::optional<std::variant<AND, NOT>>, Suffix>;
using Sequence = std::vector<Prefix>;

struct ExpressionDef : Def<std::tuple<Sequence, std::vector<std::tuple<SLASH, Sequence>>>> {};

using Definition = std::tuple<Identifier, LEFTARROW, wrap<ExpressionDef>>;
using Grammar    = std::tuple<Spacing, std::vector<Definition>, EndOfFile>;
//clang-format on

} // namespace peg
