-- Copyright 2020-2026 The Defold Foundation
-- Copyright 2014-2020 King
-- Copyright 2009-2014 Ragnar Svensson, Christian Murray
-- Licensed under the Defold License version 1.0 (the "License"); you may not use
-- this file except in compliance with the License.
--
-- You may obtain a copy of the License, together with FAQs at
-- https://www.defold.com/license
--
-- Unless required by applicable law or agreed to in writing, software distributed
-- under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
-- CONDITIONS OF ANY KIND, either express or implied. See the License for the
-- specific language governing permissions and limitations under the License.
--
-- ---------------------------------------------------------------------------
-- Indentation fixture.
--
-- This file is the output of the bundled Lua language server's formatter, so
-- reindenting it with our own engine must leave it byte for byte identical.
-- If the two disagree, the editor's indent-on-type fights format-on-save and
-- the user's file flips back and forth on every keystroke and every save.
--
-- integration.lua-indent-test runs that formatter over this file and asserts it
-- comes back unchanged, so bumping :lua-language-server-version in project.clj
-- fails there rather than letting this drift. editor.code.script-test checks
-- our half of it without needing the server unpacked.
--
-- Cases the two disagree on are commented out below, each with the reason.
-- ---------------------------------------------------------------------------

-- ---------------------------------------------------------------------------
-- Cases 1-4: correct on dev today
-- ---------------------------------------------------------------------------

-- 1. Callback ending in `end)` -- the most common pattern in Defold code.
function case_01_callback_end_paren(self)
    timer.delay(1, false, function(self, handle, dt)
        print(dt)
    end)
    print("after")
end

-- 29. Unbalanced paren inside a line comment.
function case_29_paren_in_comment()
    print(1)
    print(2)
    print(3)
end

-- 2. Table argument ending in `})`.
function case_02_table_arg_close(self)
    local id = factory.create(pos, nil, {
        scale = 2
    })
    print(id)
end

-- 3. Multi-line function signature.
function case_03_multiline_signature(a,
                                     b)
    print(a)
end

-- 4. Multi-line local function signature.
local function case_04_local_multiline_signature(self,
                                                 message_id, message)
    print(message_id)
end

-- 9. repeat / until.
function case_09_repeat_until(i)
    repeat
        print(i)
    until done(i)
    print("x")
end

-- 10. Single-line call with a table argument.
function case_10_single_line_call()
    local id = factory.create(pos, nil, {})
    print(id)
end

-- ---------------------------------------------------------------------------
-- Cases 5-7: broken on dev today
-- ---------------------------------------------------------------------------

function init(self)
    local h = timer.delay(1, false, function()
        print("tick")
    end
    )
    print("after")
end

-- 5. Call closed on a later line -- the original bug report.
function case_05_call_closed_later()
    local pos = vmath.vector3("testing",
        1, 2, 3)
    print(pos)
end

-- 6. Line break after a comma.
function case_06_break_after_comma(self, dt)
    local ax, ay = steering.combine_axes(dx, dy,
        self.gamepad_axis.x, self.gamepad_axis.y)
    print(ax)
end

-- 7. Closing paren alone on its own line.
function case_07_close_paren_own_line()
    local x = foo(
        1, 2
    )
    print(x)
end

-- 8. Nested calls closed on the same line.
function case_08_nested_calls()
    local x = math.max(
        1, math.min(
            2, 3))
    print(x)
end

-- ---------------------------------------------------------------------------
-- Cases 11-20: expected stable on dev -- must not regress (not yet measured)
-- ---------------------------------------------------------------------------

-- 11. Anonymous function stored in a local, closed with `end`.
function case_11_anonymous_function()
    local f = function(a, b)
        return a + b
    end
    print(f(1, 2))
end

-- 12. Callback followed by more arguments after the `end`.
function case_12_callback_then_more_args()
    msg.post("#", "hello", function(self)
        print("cb")
    end, "trailing")
    print("after")
end

-- 13. Table of functions -- each entry closes with `end,`.
function case_13_table_of_functions()
    local handlers = {
        on_start = function(self)
            print("start")
        end,
        on_stop = function(self)
            print("stop")
        end
    }
    print(handlers)
end

function asdf(a,
              b,
              c,
              d)
    print("hi")
end

-- 14. elseif / else chain.
function case_14_elseif_chain(n)
    if n == 0 then
        print("zero")
    elseif n == 1 then
        print("one")
    else
        print("many")
    end
    print("after")
end

-- 15. while loop with a call in the condition.
function case_15_while_loop(self)
    while has_next(self.queue) do
        print(pop(self.queue))
    end
    print("drained")
end

-- 16. Numeric and generic for loops.
function case_16_for_loops(t)
    for i = 1, 10 do
        print(i)
    end
    for k, v in pairs(t) do
        print(k, v)
    end
    print("after")
end

-- 17. Nested tables closed together with `}}`.
function case_17_nested_tables()
    local cfg = {
        physics = {
            gravity = -9.8
        }
    }
    print(cfg)
end

-- 18. return with a call spanning lines.
function case_18_return_multiline_call()
    return vmath.vector3(
        1, 2, 3)
end

-- 19. Method call chain broken across lines.
function case_19_method_chain(self)
    local n = string.format("%d",
        1)
    print(n)
end

-- 20. Deeply nested callbacks, each closing with `end)`.
function case_20_nested_callbacks(self)
    timer.delay(1, false, function()
        timer.delay(2, false, function()
            print("inner")
        end)
        print("outer")
    end)
    print("after")
end

-- ---------------------------------------------------------------------------
-- Cases 21-26: expected broken on dev (not yet measured)
-- ---------------------------------------------------------------------------

-- 21. Condition split across lines, block keyword on the closing line.
-- Commented out: we disagree with LuaLS here. LuaLS gives the continuation of
-- an `if` condition two levels; we give it one, because our fold has no notion
-- of a condition being a continuation. Its shape was:
--     if check(a,
--             b) then
--         print(a)
--     end

-- 22. Three calls closed on one line.
function case_22_triple_close()
    local x = f(
        g(
            h(
                1)))
    print(x)
end

-- 23. Trailing comment after the closing paren.
function case_23_trailing_comment()
    local pos = vmath.vector3(
        1, 2, 3) -- close call
    print(pos)
end

-- 24. Call argument list closed on the line that opens a block.
function case_24_close_then_open(self)
    for _, v in ipairs(collect(a,
        b)) do
        print(v)
    end
    print("after")
end

-- 25. Operator continuation across lines.
function case_25_operator_continuation()
    local s = concat(
        "a",
        "b")
    print(s)
end

-- 26. Assignment continued after `=`.
-- Commented out: we disagree with LuaLS here. It indents a line continuing an
-- assignment after a bare `=`; nothing is open by then as far as our scanner is
-- concerned, so we leave it flush. Its shape was:
--     local big =
--         compute(1,
--             2)

-- ---------------------------------------------------------------------------
-- Cases 27-34: scope traps -- parens inside strings and comments (not yet measured)
-- ---------------------------------------------------------------------------

-- 27. Unbalanced open paren inside a quoted string.
function case_27_paren_in_string()
    local text = "("
    print(text)
end

-- 28. Unbalanced close paren inside a quoted string.
function case_28_close_paren_in_string()
    local text = ")"
    print(text)
end

-- 29. Unbalanced paren inside a line comment.
function case_29_paren_in_comment()
    print(1) -- (
    print(2)
end

function asdfasdf()
    local s = [[ see the note (below ]]
    print(s)
end

-- 30. Unbalanced paren inside a single-line long string.
function case_30_paren_in_long_string()
    local s = [[ see the note below
    this is a test thing(
    ]]
    print(s)
end

-- 31. Unbalanced paren inside a multi-line long string.
-- The indenter reads one line at a time, so it cannot see that this is a string.
function case_31_paren_in_multiline_long_string()
    local s = [[
    see the note (below
    ]]
    print(s)
end

-- 32. Block keyword inside a multi-line long string.
function case_32_keyword_in_long_string()
    local s = [[
    function foo()
    ]]
    print(s)
end

-- 33. Unbalanced paren inside a block comment.
--[[
ignore this (paren
]]
function case_33_paren_in_block_comment()
    print("after block comment")
end

-- 34. Escaped quote before a paren.
function case_34_escaped_quote()
    local text = "say \"(\" now"
    print(text)
end

-- ---------------------------------------------------------------------------
-- Cases 35-39: conditions, comments in signatures, and long-string traps
-- ---------------------------------------------------------------------------

-- 35. Parenthesized subexpressions in a condition.
function case_35_parens_in_condition(x, y)
    if (x == 2) and (y == 2) then
        print("both")
        return true
    end
    return false
end

-- 36. Index expression spanning lines, closed with a lone `]`.
-- Commented out: we disagree with LuaLS here. We indent what an open bracket
-- holds, the same as a brace or a parenthesis; LuaLS leaves it flush with the
-- line that opened it. Its shape was:
--     local v = t[
--     key(1)
--     ]

-- 37. Comment as the only thing after the open parenthesis. It is not a
-- parameter, so there is nothing to align the parameters under.
function case_37_comment_after_open_paren( -- [[ hint
    a,
    b
)
    print(a, b)
end

-- 38. A literal `[[` inside a long string. Long strings do not nest, so this
-- is text, not an opener.
function case_38_long_string_fake_opener()
    local s = [[
[[ not a nested string
but this is normal
]]
    print(s)
end

-- 39. The same, with the literal indented.
function case_39_long_string_fake_opener_indented()
    local s = [[
    [[ not a nested string
]]
    print(s)
end
