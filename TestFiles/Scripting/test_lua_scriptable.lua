-- Test fixture: Lua class overriding InterfaceClass virtuals.
-- Used by LuaScriptableClassTest.FixtureFile_OverridesAndSelfAccess
---@class LuaFixtureImpl : InterfaceClass
LuaFixtureImpl = {
    call_count = 0
}
function LuaFixtureImpl:get_value(str)
    -- Returns 7 plus the length of the argument.
    return 7 + #str
end
function LuaFixtureImpl:buzzer()
    -- Increments a per-instance counter and mirrors it to the C++ field.
    self.call_count = self.call_count + 1
    self:set_var(self.call_count * 10)
end
