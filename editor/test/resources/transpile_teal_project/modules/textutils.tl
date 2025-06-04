local m = {}

function m.format(format: string, injections: {string:string}): string
    local result = string.gsub(format, '{{(.*)}}', injections)
    return result 
end

return m
