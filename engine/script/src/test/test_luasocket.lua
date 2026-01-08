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

-- Tests adopted from luasocket library provided tests

function test_bind()
    local socket = require "socket"
    local u = socket.udp()
    assert(u:setsockname("*", 5088))
    u:close()
    local u = socket.udp()
    assert(u:setsockname("*", 0))
    u:close()
    local t = socket.tcp()
    assert(t:bind("*", 5088))
    t:close()
    local t = socket.tcp()
    assert(t:bind("*", 0))
    t:close()
end

function test_getaddr()
    local socket = require "socket"
    local addresses = assert(socket.dns.getaddrinfo("localhost"))
    assert(type(addresses) == 'table')

    local ipv4mask = "^%d%d?%d?%.%d%d?%d?%.%d%d?%d?%.%d%d?%d?$"
    for i, alt in ipairs(addresses) do
        if alt.family == 'inet' then
            assert(type(alt.addr) == 'string')
            assert(alt.addr:find(ipv4mask))
            assert(alt.addr == '127.0.0.1')
        end
    end
end

function test_udp()
    local socket = require"socket"
    local udp = socket.udp
    local localhost = "127.0.0.1"
    local s = assert(udp())
    assert(tostring(s):find("udp{unconnected}"))
    s:setpeername(localhost, 5061)
    s:getsockname()
    assert(tostring(s):find("udp{connected}"))
    s:setpeername("*")
    s:getsockname()
    s:sendto("a", localhost, 12345)
    s:getsockname()
    assert(tostring(s):find("udp{unconnected}"))
    s:close()
end

function continue(corout, val)
    if corout ~= nil then
        local ok, val = coroutine.resume(corout, val)
        if ok ~= true then
            print("Coroutine error: " .. val)
            assert(false)
        end
        if ok == true and val == "success" then
            return nil, true, nil
        end
        return corout, true, val
    end
    return nil, true
end

function run_client_server(server, client)
    -- These coroutines take turn during a limited amount of iterations,
    -- what is yield():ed from one is passed to the next one.
    --
    -- Server runs first and thus can pass the port it listens to to the
    -- client, by yielding the port once first thing it does.
    local max_iterations = 20
    while client ~= nil or server ~= nil do
        max_iterations = max_iterations - 1
        assert(max_iterations > 0)
        local ok, val
        server, ok, val = continue(server, val)
        assert(ok)
        client, ok, val = continue(client, val)
        assert(ok)
    end
end

function tcp_server_cr(message)
    -- coroutine that listens for incoming connection on a socket
    -- for a limited time, and expects to get on connection attempt.
    local socket = require "socket";
    local listener = socket.tcp()
    assert(listener:bind("localhost", 0));
    assert(listener:listen())
    listener:settimeout(0)

    local addr, port = listener:getsockname()
    coroutine.yield(port)

    local wait_iterations = 20 -- iterations before considering this fail
    local client = nil
    while wait_iterations > 0  do
        local rdy = socket.select({listener}, {}, 0.10)
        if table.getn(rdy) == 0 then
            wait_iterations = wait_iterations - 1
        else
            client = listener:accept()
        end
        if client ~= nil then
            -- end waiting loop
            break
        end
    end

    -- asserting will end the coroutine yielding false which will propagate to continue function
    assert(wait_iterations > 0)
    assert(client ~= nil)
    client:send(message .. "\n")
    coroutine.yield()

    local data = client:receive()
    assert(data == message)

    client:close()
    listener:close()

    coroutine.yield("success")
end

function tcp_client_cr(message, port)
    local socket = require "socket"
    local conn = assert(socket.connect("localhost", port))
    coroutine.yield()
    local data = conn:receive()

    -- send it back
    assert(data == message)
    conn:send(message .. "\n")
    coroutine.yield()
    conn:close()
    coroutine.yield("success")
end

function test_tcp_clientserver()
    local test_message = "I couldn't say where she's coming from, but I just met a lady called dynamo_home!"

    local server = coroutine.create(function()
        tcp_server_cr(test_message)
    end)

    local client = coroutine.create(function(port)
        tcp_client_cr(test_message, port)
    end)

    run_client_server(server, client)
end

function udp_server_cr(message)
    local socket = require "socket"
    local u = socket.udp()
    assert(u:setsockname("127.0.0.1", 0))
    local addr, port = u:getsockname()

    coroutine.yield(port)

    -- read message from client and send back to
    -- where it came from
    local data, addr, port = u:receivefrom(4096)
    assert(data == message)

    u:sendto(message, "127.0.0.1", port)
    coroutine.yield()

    -- done
    u:close()
    coroutine.yield("success")
end

function udp_client_cr(message, port)
    -- setup & send
    local socket = require "socket"
    local u = socket.udp()
    assert(u:setsockname("127.0.0.1", 0))

    -- send first mesasge to server
    u:sendto(message, "127.0.0.1", port)
    coroutine.yield()

    -- response must be equal
    local data = u:receive(4096)
    assert(data == message)
    u:close()
    coroutine.yield("success")
end

function test_udp_clientserver()
    local test_message = "UDP Transport message!"

    local server = coroutine.create(function()
        udp_server_cr(test_message)
    end)

    local client = coroutine.create(function(port)
        udp_client_cr(test_message, port)
    end)

    run_client_server(server, client)
end

function test_bind_error()
     local socket = require "socket"

     -- bind to available port...
     local listen1 = socket.tcp()
     assert(listen1:bind("localhost", 0))
     local addr, port = listen1:getsockname()

     -- ..and try binding to it with a second socket
     local listen2 = socket.tcp()
     local ret, err = listen2:bind("localhost", port)

     -- must fail!
     assert(ret == nil)

     listen1:close()
     listen2:close()
end


functions = { test_getaddr = test_getaddr, test_bind = test_bind,
    test_udp = test_udp, test_tcp_clientserver = test_tcp_clientserver,
    test_udp_clientserver = test_udp_clientserver, test_bind_error = test_bind_error }
