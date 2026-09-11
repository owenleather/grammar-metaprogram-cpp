#include "examples/peg/syntax.h"
#include "examples/peg/node_printer.h"

const std::string peg_grammar = R"(
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

Literal <- ['] (!['] Char)* ['] Spacing
/ ["] (!["] Char)* ["] Spacing
Class <- '[' (!']' Range)* ']' Spacing
Range <- Char '-' Char / Char
Char <- '\\' [nrt'"\[\]\\]
/ '\\' [0-2][0-7][0-7]
/ '\\' [0-7][0-7]?
/ !'\\' .

LEFTARROW <- '<-' Spacing
SLASH <- '/' Spacing
AND <- '&' Spacing
NOT <- '!' Spacing
QUESTION <- '?' Spacing
STAR <- '*' Spacing
PLUS <- '+' Spacing
OPEN <- '(' Spacing
CLOSE <- ')' Spacing
DOT <- '.' Spacing

Spacing <- (Space / Comment)*
Comment <- '#' (!EndOfLine .)* EndOfLine
Space <- ' ' / '\t' / EndOfLine
EndOfLine <- '\r\n' / '\n' / '\r'
EndOfFile <- !.
)";

// TODO (owen): It would be cool if I can codegen a peg metaprogram of peg code using the peg metaprogram

int main() {
  language::Context ctx{.input = peg_grammar};

  auto result = language::Matcher<peg::Grammar>::Match(ctx);

  if (!result) {
    throw std::runtime_error("Parse failed!\n");
  }

  language::GenericNodePrinter{}(result->value);
}