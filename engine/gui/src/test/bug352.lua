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

local state = {}
state.nodes = {}
state.score = 0
state.target_score = 0

function init(self)
    local n = gui.new_text_node(vmath.vector3(15, 60, 0), "" .. state.score)
    gui.set_font(n, "big_score")
    state.nodes.score = n
end

function update(self, dt)
end

local function fade_done(self, node)
    gui.delete_node(node)
end

function on_message(self, message_id, message)
    state.target_score = state.target_score + message.score
    local p = vmath.vector3(0, 0, 0)
    p.y = 640 - p.y
    local n = gui.new_text_node(p, tostring(message.score))
    gui.set_font(n, "score")
    local start_color = vmath.vector4(1, 0, 0, 0.9)
    local end_color = vmath.vector4(1, 0, 0, 0.0)
    gui.set_color(n, start_color)
    gui.animate(n, gui.PROP_COLOR, end_color, gui.EASING_NONE, 1, 0, fade_done)
end
