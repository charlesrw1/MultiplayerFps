-- Test fixture: Lua class without C++ parent.
-- Safe to load without full ClassBase initialization because there is no
-- inheritance, so set_superclass (and thus find_class) is never called.

---@class TestNoInherit
TestNoInherit = {
    ---@type number
    value = 100,
    ---@type string
    label = "",
}
