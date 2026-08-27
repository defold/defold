local attributes = editor.external_file_attributes("README.md")
if attributes.exists and attributes.is_file then
    pprint(attributes.path)
end

local response = http.request("https://example.com/data.json", {as = "json"})
pprint(response.status, response.body)

http.server.route("/health", function(request)
    pprint(request.path, request.method, request.headers)
    return 200
end)

zip.pack("build.zip", {
    "game.project",
    {"assets", method = zip.METHOD.STORED},
})
zip.unpack("build.zip", {on_conflict = zip.ON_CONFLICT.OVERWRITE})
zip.unpack("build.zip", {"game.project"})

editor.create_resources({
    {"/generated.script", "print('generated')"},
})

editor.transact({
    editor.tx.add("/main/main.collection", "instances", {
        id = "generated",
    }),
})

local text_component = editor.ui.component(function(props)
    return editor.ui.label({text = props.text})
end)
text_component({text = "Hello"})

editor.command({
    label = "Inspect Argument",
    locations = {"Bundle", "Help"},
    query = {argument = true},
    active = function(opts)
        return opts.argument ~= nil
    end,
    run = function(opts)
        pprint(opts.argument)
    end,
})
