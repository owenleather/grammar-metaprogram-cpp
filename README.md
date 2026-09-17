# Parsing Expression Grammar (PEG) library as a C++ metaprogramming API
## Overview
Rules define C++ types directly. This makes rule definition, matching, and binding all statically typed. 
```cpp
// Each Regex<"..."> is a unique type
using Plus = Regex<"\\+">;
using Minus = Regex<"\\-">;
using Number = Regex<"[0-9]+">;

// std::tuple represents a sequence of rules
using Addition = std::tuple<Number, Plus, Number>; 
Matcher<Addition>::Match({.input = "4+3"})->value // -> ReturnType is std::tuple<Number, Plus, Number>

// std::variant represents an OR
using Op = std::variant<Plus, Minus>;
Matcher<Op>::Match({.input = "-"})->value // -> ReturnType is std::variant<Plus, Minus>

// std::vector matches a to a repeated sequence (none or many)
using Ops = std::vector<Op>;
Matcher<Ops>::Match({.input = "+-+-+-+"})->value // -> ReturnType is std::vector<std::variant<Plus, Minus>>

// std::optional represents an optional rule which may be matched or not
using MaybeOp = std::optional<Op>;
Matcher<MaybeOp>::Match({.input = "+"})->value // -> ReturnType is std::optional<std::variant<Plus, Minus>>
```

## Design
See [DESIGN.md](DESIGN.md) for the philosophy and design of this library. 