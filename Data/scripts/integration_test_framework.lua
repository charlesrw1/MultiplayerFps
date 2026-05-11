-- ---- Lua integration test framework ----
--
-- Tests register themselves at file scope by calling `add_test(name, fn)`.
-- Test functions are coroutines: yield N (seconds) to wait, raise an error to
-- fail. The C++ TestRunner calls `_lua_run_all_tests()` from a coroutine and
-- ticks it each frame, taking yielded seconds as the wait interval.

local _tests = {}
function add_test(name, fn)
    table.insert(_tests, {name=name, fn=fn})
end

-- C++ assigns _test_patterns before driving the runner. nil/empty = all tests.
_test_patterns = nil

local function _matches(name)
    if _test_patterns == nil or #_test_patterns == 0 then return true end
    for _, p in ipairs(_test_patterns) do
        local lua_pat = "^" .. p:gsub("([%^%$%(%)%%%.%[%]%+%-%?])", "%%%1"):gsub("%*", ".*") .. "$"
        if name:match(lua_pat) then return true end
    end
    return false
end

-- Driven from a coroutine by C++ TestRunner. Reports each result via
-- LuaTestRunner.report and signals completion via LuaTestRunner.set_done.
function _lua_run_all_tests()
    for _, t in ipairs(_tests) do
        if _matches(t.name) then
            local co = coroutine.create(t.fn)
            local ok, err = true, nil
            while coroutine.status(co) ~= "dead" do
                local success, val = coroutine.resume(co)
                if not success then ok = false; err = val; break end
                if val and val > 0 then coroutine.yield(val) end
            end
            LuaTestRunner.report(t.name, ok, ok and "" or tostring(err))
        end
    end
    LuaTestRunner.set_done()
end
