-- Test environment for testing the tracking code.

local __requests = {};
local __head = 1;
local __tail = 1;
local __num_saves = 0
__saves = {};

-- snippet from http://lua-users.org/wiki/CopyTable
function deepcopy(orig)
    local orig_type = type(orig)
    local copy
    if orig_type == 'table' then
        copy = {}
        for orig_key, orig_value in next, orig, nil do
            copy[deepcopy(orig_key)] = deepcopy(orig_value)
        end
        setmetatable(copy, deepcopy(getmetatable(orig)))
    else -- number, string, boolean, etc
        copy = orig
    end
    return copy
end

function test_setup_hooks()
    sys.get_save_file = function(app_id, file_name)
        return app_id .. "_" .. file_name;
    end
    http.request = function(url, method, callback, headers, post_data)
        local req = {}
        req.url = url;
        req.method = method;
        req.callback = callback;
        req.headers = headers;
        req.post_data = post_data;
        table.insert(__requests, req);
        __head = __head + 1;
    end
    sys.save = function(fn, table)
        __saves[fn] = deepcopy(table)
        __num_saves = __num_saves + 1
        return true
    end
    sys.load = function(fn)
        if __saves[fn] then
            return deepcopy(__saves[fn])
        end
        return {}
    end
end

function test_setup_save_fail()
    sys.save = function(fn, table)
        error("Generating error in save!", 1)
    end
end

function test_setup_load_fail()
    sys.load = function(fn)
        error("Generating error in load!", 1)
    end
end

function test_assert_has_request()
    assert(__head > __tail);
end

function test_assert_has_no_request()
    assert(__head == __tail)
end

function test_assert_request_url(url)
    assert(__head > __tail)
    assert(__requests[__tail].url == url)
end

function test_assert_request_stid(stid)
    test_assert_has_request()
    local req = json.decode(__requests[__tail].post_data)
    local hdr = __requests[__tail].headers
    assert(hdr["x-stid"] == stid)
end

function test_has_event(evt)
    for i=__tail,(__head-1) do
        local req = json.decode(__requests[i].post_data)
        local hdr = __requests[i].headers
        for k,v in pairs(req.events) do
            if v.type == evt then
                return true
            end
        end
    end
    return false
end

function test_assert_request_has_event(evt)
    assert(test_has_event(evt))
end

function test_assert_max_saves(count)
    assert(__num_saves <= count)
end

function test_assert_request_has_no_event(evt)
    assert(test_has_event(evt) == false)
end

function test_respond_next(status, payload, headers)
    test_assert_has_request()
    local response = {}
    response.status = status
    response.response = payload
    response.headers = headers
    __requests[__tail].callback(nil, __tail, response);
    __tail = __tail + 1
end

function test_respond_with_config(status, payload)
    local headers = {};
    test_respond_next(status, payload, headers);
end

function test_respond_with_stid(status, payload)
    local headers = {};
    test_respond_next(status, payload, headers);
end

function test_respond_with_blank(status)
    local headers = {};
    test_respond_next(status, "", headers);
end

function test_respond_with_failure(status)
    test_respond_next(status, nil, {});
end
