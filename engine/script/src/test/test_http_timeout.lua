function callback(response)
end

requests_left = 1

-- NOTE: We test timeout in a separate file as the test-server is single threaded
-- and could potentially timeout other requests

function test_http_timeout()
    local headers = {}
    http.request("http://localhost:" .. PORT .. "/sleep", "GET",
        function(response)
            assert(response.status == 0)
            requests_left = requests_left - 1
        end,
    headers)
end

functions = { test_http_timeout = test_http_timeout }
