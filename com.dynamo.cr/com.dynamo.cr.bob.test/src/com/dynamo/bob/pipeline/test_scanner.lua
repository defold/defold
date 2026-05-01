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


-- TEST SOME NORMAL REQUIRE STATEMENTS WITH SINGLE OR DOUBLE QUOTES WITH OR WITHOUT PARENTHESIS AND ADDITIONAL SPACE
require "a"
require ("b")
require "c.d"
require (   "e.f")
require 'g'
require ('h')
require 'i.j'
require (   'k.l')
require "m.n.o"
require (  "p.q.r"     )

-- REQUIRE WITH A COMMENT
require "s" -- comment
require ("t") -- comment

-- REQUIRE IN COMMENTS SHOULD NOT BE INCLUDED
--require "foo"
--require "foo.bar"
--[[
--require "bar.foo"
--]]
--[[
require "bar.foo"
]]--

-- REQUIRE IN STRINGS SHOULD NOT BE INCLUDED
local s = 'foobar require "should_not_match1"'
local s = 'foobar require("should_not_match2").foo'
local s = "foobar require 'should_not_match3'"
local s = "foobar require('should_not_match4').foo"

-- REQUIRE WITH ASSIGNMENT
local my_foo1 = require "foo1"
local my_foo2 = require("foo2")
local my_foo3 = require 'foo3'
local my_foo4 = require('foo4')

-- REQUIRE WITH ASSIGNMENT AND SAME LINE ACCESS
local foo = require("foo5").foo
local foo = require("foo6")   .foo
local foo = require("foo7").foo -- comment

-- REQUIRE IN TABLE DECLARATIONS
local t = {
    ["foo8"] = require('foo8'),
    ["foo9"] = require('foo9')  ,
    ["foo10"] = require('foo10'), -- comment
    ["foo11"] = require('foo11').foo,
    ["foo12"] = require('foo12').foo, -- comment
    ["foo13"] = require('foo13')
}

-- Issue 6111
local foo = require('foo14')("ABC")
local foo = require('foo15') {
    some_table = "abc"
}

-- Issue 6438
local foo = _G.require('foo16')

-- REQUIRE USING A VARIABLE SHOULD NOT BE INCLUDED
local modname = "foo.bar"
local module = require(modname)

-- FROM the jumper pathfinding library. SHOULD NOT BE INCLUDED
local Heuristic = require ((...):gsub('%.path$','.heuristics'))

-- Lua standard libraries + Defold extras SHOULD NOT BE INCLUDED
local coroutine = require "coroutine"
local package = require "package"
local string = require "string"
local table = require "table"
local math = require "math"
local io = require "io"
local os = require "os"
local debug = require "debug"
local socket = require "socket"
local bit = require "bit"
local ffi = require "ffi"
local jit = require "jit"
