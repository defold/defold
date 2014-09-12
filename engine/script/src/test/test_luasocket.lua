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

functions = { test_getaddr = test_getaddr, test_bind = test_bind,
              test_udp = test_udp }

