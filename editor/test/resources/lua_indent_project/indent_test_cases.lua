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
-- Baseline cases retained from the previous indentation engine
-- ---------------------------------------------------------------------------

-- Callback ending in `end)` -- the most common pattern in Defold code.
function callback_end_paren(self)
    timer.delay(1, false, function(self, handle, dt)
        print(dt)
    end)
    print("after")
end

-- Table argument ending in `})`.
function table_arg_close(self)
    local id = factory.create(pos, nil, {
        scale = 2
    })
    print(id)
end

-- Multi-line function signature.
function multiline_signature(a,
                             b)
    print(a)
end

-- Multi-line local function signature.
local function local_multiline_signature(self,
                                         message_id, message)
    print(message_id)
end

-- repeat / until.
function repeat_until(i)
    repeat
        print(i)
    until done(i)
    print("x")
end

-- Single-line call with a table argument.
function single_line_call()
    local id = factory.create(pos, nil, {})
    print(id)
end

-- ---------------------------------------------------------------------------
-- Primary continuation regressions fixed by the new indentation engine
-- ---------------------------------------------------------------------------

function init(self)
    local h = timer.delay(1, false, function()
        print("tick")
    end
    )
    print("after")
end

-- Call closed on a later line -- the original bug report.
function call_closed_later()
    local pos = vmath.vector3("testing",
        1, 2, 3)
    print(pos)
end

-- Line break after a comma.
function break_after_comma(self, dt)
    local ax, ay = steering.combine_axes(dx, dy,
        self.gamepad_axis.x, self.gamepad_axis.y)
    print(ax)
end

-- Closing paren alone on its own line.
function close_paren_own_line()
    local x = foo(
        1, 2
    )
    print(x)
end

-- Nested calls closed on the same line.
function nested_calls()
    local x = math.max(
        1, math.min(
            2, 3))
    print(x)
end

-- ---------------------------------------------------------------------------
-- Additional regression coverage
-- ---------------------------------------------------------------------------

-- Anonymous function stored in a local, closed with `end`.
function anonymous_function()
    local f = function(a, b)
        return a + b
    end
    print(f(1, 2))
end

-- Callback followed by more arguments after the `end`.
function callback_then_more_args()
    msg.post("#", "hello", function(self)
        print("cb")
    end, "trailing")
    print("after")
end

-- Table of functions -- each entry closes with `end,`.
function table_of_functions()
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

-- elseif / else chain.
function elseif_chain(n)
    if n == 0 then
        print("zero")
    elseif n == 1 then
        print("one")
    else
        print("many")
    end
    print("after")
end

-- while loop with a call in the condition.
function while_loop(self)
    while has_next(self.queue) do
        print(pop(self.queue))
    end
    print("drained")
end

-- Numeric and generic for loops.
function for_loops(t)
    for i = 1, 10 do
        print(i)
    end
    for k, v in pairs(t) do
        print(k, v)
    end
    print("after")
end

-- Nested tables, each closing brace on its own line.
function nested_tables()
    local cfg = {
        physics = {
            gravity = -9.8
        }
    }
    print(cfg)
end

-- return with a call spanning lines.
function return_multiline_call()
    return vmath.vector3(
        1, 2, 3)
end

-- Assignment from a call whose arguments wrap onto the next line.
function call_args_wrapped()
    local n = string.format("%d",
        1)
    print(n)
end

-- Deeply nested callbacks, each closing with `end)`.
function nested_callbacks(self)
    timer.delay(1, false, function()
        timer.delay(2, false, function()
            print("inner")
        end)
        print("outer")
    end)
    print("after")
end

-- ---------------------------------------------------------------------------
-- Close/open combinations and known formatter differences
-- ---------------------------------------------------------------------------

-- Condition split across lines, block keyword on the closing line.
-- Commented out: we disagree with LuaLS here. LuaLS gives the continuation of
-- an `if` condition two levels; we give it one, because our fold has no notion
-- of a condition being a continuation. Its shape was:
--     if check(a,
--             b) then
--         print(a)
--     end

-- Three calls closed on one line.
function triple_close()
    local x = f(
        g(
            h(
                1)))
    print(x)
end

-- Trailing comment after the closing paren.
function trailing_comment()
    local pos = vmath.vector3(
        1, 2, 3) -- close call
    print(pos)
end

-- Call argument list closed on the line that opens a block.
function close_then_open(self)
    for _, v in ipairs(collect(a,
        b)) do
        print(v)
    end
    print("after")
end

-- Call with every argument on a continuation line.
function call_args_one_per_line()
    local s = concat(
        "a",
        "b")
    print(s)
end

-- Assignment continued after a trailing `=`. The continuation lines indent one
-- level, and the call opened on the first of them indents its arguments again.
function assign_continued_after_equals()
    local big =
        compute(1,
            2)
    print(big)
end

-- A blank line inside a continuation does not end it.
function assign_continued_across_blank_line()
    local spaced =

        compute(1)
    print(spaced)
end

-- A continuation inside a table constructor ends at the value, so the closing
-- brace still lines up with the line that opened it.
function assign_continued_inside_table()
    local t = {
        x =
            1,
        y = 2
    }
    print(t)
end

-- ---------------------------------------------------------------------------
-- Scope traps -- parens inside strings and comments
-- ---------------------------------------------------------------------------

-- Unbalanced open paren inside a quoted string.
function paren_in_string()
    local text = "("
    print(text)
end

-- Unbalanced close paren inside a quoted string.
function close_paren_in_string()
    local text = ")"
    print(text)
end

-- Unbalanced paren inside a line comment.
function paren_in_comment()
    print(1) -- (
    print(2)
end

-- Unbalanced paren inside a long string that opens with content on the same
-- line as the `[[`.
function paren_in_long_string()
    local s = [[ see the note below
    this is a test thing(
    ]]
    print(s)
end

-- Unbalanced paren inside a long string that opens on its own line.
-- Long-string state is carried across lines, so the paren remains string content.
function paren_in_multiline_long_string()
    local s = [[
    see the note (below
    ]]
    print(s)
end

-- Block keyword inside a multi-line long string.
function keyword_in_long_string()
    local s = [[
    function foo()
    ]]
    print(s)
end

-- Unbalanced paren inside a block comment.
--[[
ignore this (paren
]]
function paren_in_block_comment()
    print("after block comment")
end

-- Escaped quote before a paren.
function escaped_quote()
    local text = "say \"(\" now"
    print(text)
end

-- ---------------------------------------------------------------------------
-- Conditions, comments in signatures, and long-string traps
-- ---------------------------------------------------------------------------

-- Parenthesized subexpressions in a condition.
function parens_in_condition(x, y)
    if (x == 2) and (y == 2) then
        print("both")
        return true
    end
    return false
end

-- Index expression spanning lines, closed with a lone `]`.
-- Commented out: we disagree with LuaLS here. We indent what an open bracket
-- holds, the same as a brace or a parenthesis; LuaLS leaves it flush with the
-- line that opened it. Its shape was:
--     local v = t[
--     key(1)
--     ]

-- Comment as the only thing after the open parenthesis. It is not a
-- parameter, so there is nothing to align the parameters under.
function comment_after_open_paren( -- [[ hint
    a,
    b
)
    print(a, b)
end

-- A literal `[[` inside a long string. Long strings do not nest, so this
-- is text, not an opener.
function long_string_fake_opener()
    local s = [[
[[ not a nested string
but this is normal
]]
    print(s)
end

-- The same, with the literal indented.
function long_string_fake_opener_indented()
    local s = [[
    [[ not a nested string
]]
    print(s)
end
