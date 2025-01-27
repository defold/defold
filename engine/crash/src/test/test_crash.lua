-- Copyright 2020-2025 The Defold Foundation
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

function test_crash()

    -- dummy load, after this should be purged
    local u = crash.load_previous();
    if u ~= nil then
        crash.release(u);
    end
    
    -- crash should not be available any longer
    local v = crash.load_previous();
    assert(n ~= 0);

    -- test reloading 
    local userdata = {
    	"This is the first user field",
    	"These fields can contain user defined application state",
    	"Such as breadcrumbs and so on",
    	"Maybe even user-id:s when they are logged on, user-123456",
    	
    	"255b255b255b255b255b255b255b255b255b255b255b255b255b255b255b255b" ..
    	"255b255b255b255b255b255b255b255b255b255b255b255b255b255b255b255b" ..
    	"255b255b255b255b255b255b255b255b255b255b255b255b255b255b255b255b" ..
    	"255b255b255b255b255b255b255b255b255b255b255b255b255b255b255b255",

    	"256b256b256b256b256b256b256b256b256b256b256b256b256b256b256b256b" ..
    	"256b256b256b256b256b256b256b256b256b256b256b256b256b256b256b256b" ..
    	"256b256b256b256b256b256b256b256b256b256b256b256b256b256b256b256b" ..
    	"256b256b256b256b256b256b256b256b256b256b256b256b256b256b256b256b",
    	
    	"Here is a long long long long long long long long long long long long long long long " ..
    	"long long long long long long long long long long long long long long long long long " ..
    	"long long long long long long long long long long long long long long long long long " ..
    	"long long long long long long long long long long long long long long long long long " ..
    	"long long long long long long long long long long long long long long long long long " ..
    	"long long long long long long long long long long long long long long long long long " ..
    	"long long long long long long long long long long long long long long long long long " ..
    	"long long long long long long long long long long long long long long long long long " ..
    	"long long long long long long long long long long long long long long long long long " ..
    	"long long long long long long long long long long long long long long long long long " ..
    	"long long long long long long long long long long long long long long long long long string."
    }
    
    crash.set_user_field(0, "Custom")
    for k,v in ipairs(userdata) do
         crash.set_user_field(k, v)
    end    
       
    crash.write_dump();
    u = crash.load_previous();
    assert(u ~= nil);
    
    assert(crash.get_signum(u) == 0xDEAD)
    assert(crash.get_user_field(u, 0) == "Custom")
    assert(crash.get_user_field(u, 1) == userdata[1])
    assert(crash.get_user_field(u, 2) == userdata[2])
    assert(crash.get_user_field(u, 3) == userdata[3])
    assert(crash.get_user_field(u, 4) == userdata[4])
    assert(crash.get_user_field(u, 5) == userdata[5]) -- 255
    assert(string.len(crash.get_user_field(u, 6)) == 255) -- 256 but truncated
    assert(crash.get_user_field(u, 7) ~= "") -- the long one
    assert(crash.get_user_field(u, 8) == "")
    assert(crash.get_sys_field(u, crash.SYSFIELD_ENGINE_VERSION) == "DefoldScriptTest")
    assert(type(crash.get_modules(u)) == "table")
    assert(type(crash.get_backtrace(u)) == "table")
end

functions = { test_crash = test_crash }
