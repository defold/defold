
function random_value()
    local t = math.random(0,1)
    local value = 0
    if t == 0 then
        value = math.random(0, 1000)
    elseif t == 1 then
        value = "" .. math.random(0, 1000)
    end
    return value
end

function random_table(depth, max_depth)
    local ret = {}
    if depth >= max_depth-1 then
        return ret
    end

    local n = math.random(8, 16)
    local t = {}
    --print(string.rep(" ", depth) .. "n", n)
    for i = 1,n do
        local t = math.random(0,1)
        local key = "" .. math.random(0, 1000)
        local value = 0
        if t == 0 then
            value = random_value()
        elseif t == 1 then
            value = random_table(depth+1, max_depth)
        end
        ret[key] = value
        --print(string.rep(" ", depth) .. key, value)
    end
    return ret
end

function test_pickle1()
    local t = { a = 1, b = 2, c = 3 }
    local t_prim = pickle.loads(pickle.dumps(t))

    assert(t['a'] == t_prim['a'])
    assert(t['b'] == t_prim['b'])
    assert(t['c'] == t_prim['c'])

    local f = io.open("/tmp/foobar", "w")
    f:write(pickle.dumps(t))
    f:close()
end

function table_count(t)
    local n = 0
    for i,v in pairs(t) do
        n = n + 1
    end
    return n
end

function compare_table(t, t_prim)
    local n1 = table_count(t)
    local n2 = table_count(t_prim)
    assert(n1 == n2, n1 .. "!=" .. n2)
    for i,v in pairs(t) do
        if type(v) == "table" then
            compare_table(v, t_prim[i])
        else
            assert(v == t_prim[i], tostring(v) .. "!=" .. tostring(t_prim[i]))
        end
    end
end

function test_pickle2()
    local t = random_table(0, 5)
    local t_prim = pickle.loads(pickle.dumps(t))
    compare_table(t, t_prim)
end

function test_pickle()
    test_pickle1()
    test_pickle2()
end

functions = { test_pickle = test_pickle }
