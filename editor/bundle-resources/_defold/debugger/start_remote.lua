local port = debugger.start(8172 + sys.get_config_int("project.instance_index", 0), "0.0.0.0")
print("Lua DAP debugger port: " .. port)

-- Keep under ~500 bytes
