
## Design
### Philosophy
I thought that it would be nice if I could define parsing rules that are directly types that I want out of the parser. 

For example, suppose a binary operation requires a number followed by an operator followed by a number. After parsing my source code, I'd like to receive a `std::tuple<Number, Operator, Number>` for every binary operator (or, ideally, a struct `BinaryOperator` with `.a`, `.op`, and `.b`, which is possible but I decided to skip for now). Suppose an `Operator` could be a `Plus` or `Minus`, I'd like to receive a `std::variant<Plus, Minus>` so that I can use the suite of C++ tools for handling variants. 

Therefore, defining and rules and handling results is almost the same thing. I can define
```cpp
struct BinaryOperator : std::tuple<Number, Operator, Number>{};
```
and I can use the results of parsing directly
```cpp
double Compute(const BinaryOperator& results) {
    const auto& [a, op, b] = results;
    return std::visit(OperatorVisitor{a, b}, op);
}
```

This is not always beneficial. Some rules simply inform the parser and aren't needed in the results. For example, `Spacing` is commonly used to ignore whitespace, but would still exist in the parsed results as a dummy type and need to be ignored.
```cpp
auto HandleOp(const Operator& op) {
    const auto& [op, spacing] = op;
    // spacing is unused
    ...
}
```

In practice, this philosophy often leads to overly verbose/complicated types and long compile times. For complex parsers there can be deep nesting of types. Something like a recursive descendent parser would be more efficient and probably simpler in the end. Nonetheless, this project was an interesting exercise in metaprogramming. 

### Parsing (Matching)
Parsing is implemented as a set of `Matcher` structs that implement specialized logic to match source text to its rule. 
```cpp
template <typename Rule> struct Matcher;
```
Each `Matcher` specialization must have a member function 
```cpp
static std::optional<Result<ReturnType>> Match(Context ctx);
```
This function attempts to match the source text to its rule. If the source text does not match the rule, it returns nullopt. If it does, it returns a Result which contains the matched type and an updated `Context` which contains the original source text minus the matched text.

The only matcher that actually works with the raw source text is the `Regex` matcher. This tries to match its regex to the start of the source text and either returns a `Regex` object with the consumed text or returns nullopt. Other matchers delegate to lower-level matchers  in some way until it reaches `Regex` matchers which interpret and manipulate the source text.

For example, the matcher for `std::tuple<Rules...>` matches each `Rule in Rules`. If all match successfully, it returns a tuple of the sub-matches. If any don't match, it returns nullopt for the whole rule, since a tuple must match everything. All the other matchers works similarly but with different rules for handling how to call lower-level matchers and what to return. 

