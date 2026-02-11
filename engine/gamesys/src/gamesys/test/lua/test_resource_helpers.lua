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

function make_anim(id, width, height, fstart, fend)
    return {
        id=id,
        width=width,
        height=height,
        frame_start=fstart,
        frame_end=fend
    }
end

function make_quad(w, h, x, y)
    return {
        vertices = {0, 0,
                    0, h,
                    w, h,
                    w, 0},
        uvs      = {x    , y,
                    x    , y + h,
                    x + w, x + h,
                    x + w, y},
        indices  = {0,1,2,0,2,3}
    }
end

function assert_anim(a, expected)
    assert(a.id              == expected.id)
    assert(a.width           == expected.width)
    assert(a.height          == expected.height)
    assert(a.frame_start     == expected.frame_start)
    assert(a.frame_end       == expected.frame_end)

    -- Optional entries, test only if required
    if expected.fps ~= nil then
        assert(a.fps == expected.fps)
    end
    if expected.playback ~= nil then
        assert(a.playback == expected.playback)
    end
    if expected.flip_vertical ~= nil then
        assert(a.flip_vertical == expected.flip_vertical)
    end
    if expected.flip_horizontal ~= nil then
        assert(a.flip_horizontal == expected.flip_horizontal)
    end
end

local function assert_near(exp, act)
    local epsilon = 0.000001
    assert(math.abs(exp - act) < epsilon)
end

function assert_geo(a, expected)
    assert(#a.vertices == #expected.vertices)
    assert(#a.uvs      == #expected.uvs)
    assert(#a.indices  == #expected.indices)
    for k,v in pairs(a.vertices) do
        assert_near(v, expected.vertices[k])
    end
    for k,v in pairs(a.uvs) do
        assert_near(v, expected.uvs[k])
    end
    for k,v in pairs(a.indices) do
        assert_near(v, expected.indices[k])
    end
end
