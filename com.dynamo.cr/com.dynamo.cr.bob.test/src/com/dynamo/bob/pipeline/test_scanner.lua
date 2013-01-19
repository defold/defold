
require "a"
require ("a")
require "a/b"
require (   "a/b")
--require "foo"
--require "foo/bar"
--[[
--require "bar/foo"
--]]

local s = 'require "should_not_match"'

require "a/b/c"
require (  "a/b/c"     )
