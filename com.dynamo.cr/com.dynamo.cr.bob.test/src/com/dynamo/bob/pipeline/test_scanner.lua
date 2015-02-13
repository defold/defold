
require "a"
require ("a")
require "a/b"
require (   "a/b")
require 'a'
require ('a')
require 'a/b'
require (   'a/b')
--require "foo"
--require "foo/bar"
--[[
--require "bar/foo"
--]]

local s = 'require "should_not_match"'

require "a/b/c"
require (  "a/b/c"     )

local my_foo1 = require "foo"
local my_foo2 = require("foo")
local my_foo3 = require 'foo'
local my_foo4 = require('foo')
