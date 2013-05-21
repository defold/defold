function callback(response)
end

requests_left = 4

function test_http()
    local headers = {}
    headers['X-A'] = 'Defold'
    headers['X-B'] = '!'
    http.request("http://localhost:" .. PORT, "GET",
        function(response)
            assert(response.status == 200)
            assert(response.response == "Hello Defold!")
            assert(response.headers.server == "Dynamo 1.0")
            assert(response.headers['transfer-encoding'] == "chunked")
            requests_left = requests_left - 1
        end,
    headers)

    local post_data = "Some data..."
    http.request("http://localhost:" .. PORT, "POST",
        function(response)
            assert(response.status == 200)
            assert(response.response == "PONGSome data...")
            assert(response.headers.server == "Dynamo 1.0")
            assert(response.headers['transfer-encoding'] == "chunked")
            requests_left = requests_left - 1
        end,
    headers, post_data)


    http.request("http://localhost:" .. PORT .. "/not_found", "GET",
        function(response)
            assert(response.status == 404)
            assert(response.response == "")
            assert(response.headers.server == "Dynamo 1.0")
            assert(response.headers['transfer-encoding'] == "chunked")
            requests_left = requests_left - 1
        end,
    headers)

    http.request("http://foo.___", "GET",
        function(response)
            assert(response.status == 0)
            assert(response.response == "")
            assert(#response.response == 0)
            requests_left = requests_left - 1
        end,
    headers)


end

functions = { test_http = test_http }
