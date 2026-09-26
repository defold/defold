local port = debugger.start(8172 + sys.get_config_int("project.instance_index", 0))
print("Lua DAP debugger listening on 127.0.0.1:" .. port)

-- Keep under ~500 bytes
