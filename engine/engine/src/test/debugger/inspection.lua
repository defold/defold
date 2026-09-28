-- Copyright 2020-2026 The Defold Foundation
-- Copyright 2014-2020 King
-- Copyright 2009-2014 Ragnar Svensson, Christian Murray
-- Licensed under the Defold License version 1.0 (the "License"); you may not use
-- this file except in compliance with the License.
--
-- You may obtain a copy of the License, together with FAQs at
-- https://www.defold.com/license
--
-- Unless required by applicable law or agreed to in writing, software distributed
-- under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
-- CONDITIONS OF ANY KIND, either express or implied. See the License for the
-- specific language governing permissions and limitations under the License.

local M = {}

-- The DAP client edits these fields before allowing the callback to continue.
local function inspect(self)
    self.score = 41
    self.items = {"one"}
    self.cycle = self
    self.action = function() end
    local calls = 0
    local yielded = sys.get_config_int("test.debugger_yielded", 0) == 1
    local function run(self)
        local opaque = newproxy(true)
        local mt = getmetatable(opaque)
        mt.__index = function() calls = calls + 1 end
        mt.__tostring = function() calls = calls + 1; return "opaque" end
        mt.__get_instance_data_table_ref = function() calls = calls + 1 end
        if yielded then
            coroutine.yield()
        end
        self.score = self.score -- inspect
        assert(self.score == 99 and self.items[1] == "two" and self.label == "done")
        assert(self.cycle == self and opaque ~= nil and calls == 0)
    end
    if yielded then
        local co = coroutine.create(run)
        assert(coroutine.resume(co, self))
        local marker = 1 -- suspended
        assert(coroutine.resume(co))
    else
        run(self)
    end
    assert(calls == 0)
end

local function replaced_getter(self)
    self.score = 41
    local calls = 0
    local mt = debug.getmetatable(self)
    local getter = mt.__get_instance_data_table_ref
    local replacement = function() calls = calls + 1; return getter(self) end
    local marker = 1 -- getter
    assert(calls == 0 and self.score == 41 and marker == 1)
end

-- Inspect node subtype names without running metamethods, including while yielded.
local function gui_nodes(self)
    local yielded = sys.get_config_int("test.debugger_yielded", 0) == 1
    local function run()
        local box = gui.new_box_node(vmath.vector3(852, 320, 0), vmath.vector3(100, 100, 0))
        local text = gui.new_text_node(vmath.vector3(803, 336, 0), 'Score "42"\\ready\nnext')
        local pie = gui.new_pie_node(vmath.vector3(1, 2, 3), vmath.vector3(10, 10, 0))
        local custom = gui.new_debugger_test_node()
        local deleted = gui.new_box_node(vmath.vector3(), vmath.vector3(1))
        gui.delete_node(deleted)
        self.nodes = {box = box, text = text, pie = pie, custom = custom, deleted = deleted}
        local unknown = newproxy(true)
        getmetatable(unknown).__tostring = type
        local calls = 0
        local mt = debug.getmetatable(box)
        local original = mt.__tostring
        local replacement = function() calls = calls + 1; return "unexpected" end
        if yielded then
            coroutine.yield()
        end
        local marker = 1 -- gui-nodes-inspect
        assert(mt.__tostring == original and calls == 0 and marker == 1)
        assert(gui.get_position(box).x == 852)
    end
    if yielded then
        local co = coroutine.create(run)
        assert(coroutine.resume(co))
        local marker = 1 -- gui-nodes-suspended
        assert(coroutine.status(co) == "suspended" and marker == 1)
        assert(coroutine.resume(co))
    else
        run()
    end
end

-- Inspect built-in type names and identities, including in a yielded thread.
local function native_types(self)
    local yielded = sys.get_config_int("test.debugger_yielded", 0) == 1
    local function run()
        local values = {}
        if sys.get_config_string("test.debugger_instance") == "render" then
            values.constants = render.constant_buffer()
            values.predicate = render.predicate({"tile"})
        else
            local stream_name = hash('position"\\\n')
            values.buffer = buffer.create(2, {{name = stream_name, type = buffer.VALUE_TYPE_FLOAT32, count = 3}})
            values.stream = buffer.get_stream(values.buffer, stream_name)
            values.file = assert(io.tmpfile())
            values.closed_file = assert(io.tmpfile())
            values.closed_file:close()
            values.tcp = assert(socket.tcp())
            values.server = assert(socket.tcp())
            assert(values.server:bind("127.0.0.1", 0))
            assert(values.server:listen(1))
            local _, port = values.server:getsockname()
            values.client = assert(socket.tcp())
            values.client:settimeout(1)
            assert(values.client:connect("127.0.0.1", port))
            values.server:settimeout(1)
            local accepted = assert(values.server:accept())
            accepted:close()
            values.udp = assert(socket.udp())
            values.connected_udp = assert(socket.udp())
            assert(values.connected_udp:setpeername("127.0.0.1", port))
            if sys.get_config_string("physics.type") == "3D" then
                values.world = bullet3d.get_world()
                values.body = bullet3d.get_collision_object("#body")
                values.shape = bullet3d.collision_object.get_shapes(values.body)[1]
                values.constraint = bullet3d.constraint.create_point_to_point(values.body, nil, {
                    pivot_a = vmath.vector3(), pivot_b = vmath.vector3(),
                })
            else
                values.body = b2d.get_body("#body")
                values.joint = b2d.joint.create_distance(values.body, b2d.get_body("#other_body"), {
                    local_anchor_a = vmath.vector3(), local_anchor_b = vmath.vector3(), length = 1,
                })
                -- Box2D v2 worlds are lightuserdata and fixtures are tables.
                if b2d.body.get_shapes then
                    values.world = b2d.get_world()
                    values.shape = b2d.body.get_shapes(values.body)[1].shape_id
                    values.chain = b2d.body.create_chain(b2d.get_body("#other_body"), {
                        vertices = {vmath.vector3(-10, 0, 0), vmath.vector3(0, 10, 0), vmath.vector3(10, 0, 0)},
                        prev_vertex = vmath.vector3(-20, -10, 0), next_vertex = vmath.vector3(20, -10, 0),
                    })
                end
            end
        end
        if yielded then coroutine.yield() end
        local marker = 1 -- native-types-inspect
        assert(marker == 1 and next(values) ~= nil)
        if values.file then
            assert(values.file:write("still open"))
            values.file:close()
            for _, name in ipairs({"tcp", "server", "client", "udp", "connected_udp"}) do values[name]:close() end
        end
    end
    if yielded then
        local co = coroutine.create(run)
        assert(coroutine.resume(co))
        local marker = 1 -- native-types-suspended
        assert(coroutine.status(co) == "suspended" and marker == 1)
        assert(coroutine.resume(co))
    else
        run()
    end
end

-- Issue #7750 needs a real engine reboot, not just a DAP disconnect/reconnect.
-- Keep updating while detached so the client can attach to each fresh runtime.
local function reboot(self)
    self.score = 41
    local generation = sys.get_config_int("test.debugger_generation", 0)
    local port = debugger.start()
    timer.delay(0.01, true, function()
        local score = self.score -- reboot-inspect
        if self.action then
            assert(score == 99)
            if self.action == "reboot" then
                -- Reboot replaces argv; the engine requires the project last.
                sys.reboot("--config=test.debugger_generation=" .. (generation + 1),
                    "--config=debugger.port=" .. port,
                    sys.get_config_string("test.debugger_project"))
            else
                sys.exit(0)
            end
        end
    end)
end

function M.run(self, kind)
    if sys.get_config_string("test.debugger_instance") ~= kind then
        return
    end
    local case = sys.get_config_string("test.debugger_case")
    local test = inspect
    if case == "getter" then
        test = replaced_getter
    elseif case == "gui_nodes" then
        test = gui_nodes
    elseif case == "native_types" then
        test = native_types
    elseif case == "reboot" then
        test = reboot
    end
    local ok, message = pcall(test, self)
    if not ok then
        print(message)
    end
    if not ok or case ~= "reboot" then
        sys.exit(ok and 0 or 1)
    end
end

return M
