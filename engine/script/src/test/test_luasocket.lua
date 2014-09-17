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


function continue(corout, name)
    if corout ~= nil then
        local ok, val = coroutine.resume(corout)
        if ok ~= true then
            print("Coroutine error: " .. val)
            assert(false)
        end
        if ok == true and val == "success" then
            return nil, true
        end
    end
    return corout, true
end

function test_client_server(server, client)
    -- these coroutines take turn during a limited amount of iterations
    local max_iterations = 20
    while client ~= nil or server ~= nil do
        max_iterations = max_iterations - 1
        assert(max_iterations > 0)
        local ok
        server, ok = continue(server, "server")
        assert(ok)
        client, ok = continue(client, "client")
        assert(ok)
    end
end

function tcp_server_cr(port, message)
    -- coroutine that listens for incoming connection on a socket
    -- for a limited time, and expects to get on connection attempt.
    local socket = require "socket";
    local listener = socket.tcp()
    assert(listener:bind("*", port));
    assert(listener:listen())
    listener:settimeout(0)

    coroutine.yield()

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

function tcp_client_cr(port, message)
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
    -- Randomize port some so can run the test in quick succession with lower chance of
    -- running into port already in use, since that will make the test fail.
    local port = 5000 + math.floor(socket.gettime() * 10) % 1000
    local test_message = "I couldn't say where she's coming from, but I just met a lady called dynamo_home!"
    test_client_server(
    coroutine.create(function()
        tcp_server_cr(port, test_message)
    end),
    coroutine.create(function()
        tcp_client_cr(port, test_message)
    end)
    )
end

function udp_server_cr(port, message)
    local socket = require "socket"
    local u = socket.udp()

    assert(u:setsockname("127.0.0.1", port))
    coroutine.yield()

    -- send
    u:sendto(message, "127.0.0.1", port + 1)
    coroutine.yield()

    -- read back
    local data = u:receive(4096)
    assert(data == message)

    -- done
    u:close()
    coroutine.yield("success")
end

function udp_client_cr(port, message)
    -- setup & send
    local socket = require "socket"
    local u = socket.udp()
    assert(u:setsockname("127.0.0.1", port + 1))
    coroutine.yield()

    -- wait for server msg
    local data = u:receive(4096)
    assert(data == message)

    -- send one back
    u:sendto(message, "127.0.0.1", port)
    u:close()
    coroutine.yield("success")
end

function test_udp_clientserver()
    -- Port randomization here too. This opens udp socket and sends messages back and forth.
    local port = 5000 + math.floor(socket.gettime() * 10) % 1000
    local test_message = "UDP Transport message!"
    test_client_server(
    coroutine.create(function()
        udp_server_cr(port, test_message)
    end),
    coroutine.create(function()
        udp_client_cr(port, test_message)
    end)
    )
end


functions = { test_getaddr = test_getaddr, test_bind = test_bind,
    test_udp = test_udp, test_tcp_clientserver = test_tcp_clientserver,
    test_udp_clientserver = test_udp_clientserver }
