---@type string
local invalid_response = http.request("https://example.com", {})

print(invalid_response)
