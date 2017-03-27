
-- TEST SOME NORMAL REQUIRE STATEMENTS WITH SINGLE OR DOUBLE QUOTES WITH OR WITHOUT PARENTHESIS AND ADDITIONAL SPACE
require "a"
require ("a")
require "a.b"
require (   "a.b")
require 'a'
require ('a')
require 'a.b'
require (   'a.b')
require "a.b.c"
require (  "a.b.c"     )

-- REQUIRE IN COMMENTS SHOULD NOT BE INCLUDED
--require "foo"
--require "foo.bar"
--[[
--require "bar.foo"
--]]

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


