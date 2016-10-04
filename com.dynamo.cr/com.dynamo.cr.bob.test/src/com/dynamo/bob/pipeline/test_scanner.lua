
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

local my_foo1 = require "foo1"
local my_foo2 = require("foo2")
local my_foo3 = require 'foo3'
local my_foo4 = require('foo4')

local t = {
    ["foo5"] = require('foo5'),
    ["foo6"] = require('foo6')  ,
    ["foo7"] = require('foo7'), -- comment
    ["foo8"] = require('foo8')
}