function callback(response)
end

requests_left = 0

-- NOTE: We test timeout in a separate file as the test-server is single threaded
-- and could potentially timeout other requests

function test_http_timeout()
    local headers = {}
    local options = {}
    options['timeout'] = 1.0

    print("Failing http requests ahead ->")
    http.request("http://127.0.0.1:" .. PORT .. "/sleep/1.5", "GET",
        function(response)
            assert(response.status == 0)
            requests_left = requests_left - 1
        end,
    headers, '', options)
    requests_left = requests_left + 1

    -- The config file also specifies a timeout value, let's test that too
    http.request("http://127.0.0.1:" .. PORT .. "/sleep", "GET",
        function(response)
            assert(response.status == 0)
            requests_left = requests_left - 1
        end,
    headers)
    requests_left = requests_left + 1

    -- Also test one that succeeds, i.e. does not time out
    http.request("http://127.0.0.1:" .. PORT .. "/sleep/0.5", "GET",
        function(response)
            assert(response.status == 200)
            requests_left = requests_left - 1
        end,
    headers, '', options)
    requests_left = requests_left + 1

    for i=1,10 do

        options['timeout'] = 1.0
        http.request(string.format("http://127.0.0.1:" .. PORT .. "/sleep/1.10%d", i), "GET",
            function(response)
                assert(response.status == 0) -- it should timeout

                requests_left = requests_left - 1
            end,
        headers, '', options)
        requests_left = requests_left + 1
    end
end


functions = { test_http_timeout = test_http_timeout }
