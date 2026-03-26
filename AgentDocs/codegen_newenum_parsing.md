# NEWENUM Codegen Parsing

`Scripts/codegen_lib.py` parses `NEWENUM(name, type){ ... };` in two passes: `read_typenames_from_text` registers the enum name, then `parse_file` calls `parse_enum` to read values from subsequent lines.

## Edge cases fixed

**Single-line:** `NEWENUM(Foo, int){A, B, C};`
The `{...};` is on the same line as NEWENUM — `parse_enum` never sees it. Fixed in `parse_file` (line ~690): detect `{` and `};` on the same line and parse values inline.

**Brace on NEWENUM line, values on next line:** `NEWENUM(Easing, uint8_t){\n    Linear, CubicEaseIn, ...,\n};`
`parse_enum`'s `else` branch only took the first token per line (`line.split()[0]`). Fixed to split on commas and emit one `Property` per token.

## Tests
`Scripts/codegen_tests.py` — `TestNewenum` class covers multiline, single-line, and multi-value-per-line cases.

## clang-format note
VS 2019 ships clang-format ~10. `WhitespaceSensitiveMacros` (added in clang-format 12) has no effect. Use `// clang-format off/on` guards if needed, or ensure the codegen parser handles all collapsed forms.
