// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.input.proto.Input.Gamepad;
import com.dynamo.input.proto.Input.GamepadMap;
import com.dynamo.input.proto.Input.GamepadMapEntry;
import com.dynamo.input.proto.Input.GamepadMaps;
import com.dynamo.input.proto.Input.GamepadModifier;
import com.dynamo.input.proto.Input.GamepadType;
import com.google.protobuf.TextFormat;

public class GamepadConverterTest extends AbstractProtoBuilderTest {

    @Test
    public void testConvertSdlMappingToGamepadMaps() throws Exception {
        String sdl = ""
                + "03000000000000000000000000000000,Ignored Pad,a:b0,platform:Windows,\n"
                + "03000000000000000000000000000001,Test Pad,a:b0,b:b1,x:b2,y:b3,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,leftshoulder:b4,leftstick:b10,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:b7,rightx:a3,righty:a4,start:b9,platform:Mac OS X,\n";

        GamepadMaps maps = parse(GamepadConverter.convertToDefoldFormat(sdl, "x86_64-macos"));

        assertEquals(1, maps.getDriverCount());
        GamepadMap driver = maps.getDriver(0);
        assertEquals("Test Pad", driver.getDevice());
        assertEquals("macos", driver.getPlatform());
        assertTrue(driver.hasGuid());
        assertEquals(3L, Integer.toUnsignedLong(driver.getGuid().getBusCrc()));
        assertEquals(0L, Integer.toUnsignedLong(driver.getGuid().getVendorProduct()));
        assertEquals(16777216L, Integer.toUnsignedLong(driver.getGuid().getVersionDriversignatureDriverdata()));

        GamepadMapEntry a = find(driver, Gamepad.GAMEPAD_RPAD_DOWN);
        assertEquals(GamepadType.GAMEPAD_TYPE_BUTTON, a.getType());
        assertEquals(0, a.getIndex());

        GamepadMapEntry left = find(driver, Gamepad.GAMEPAD_LSTICK_LEFT);
        assertEquals(GamepadType.GAMEPAD_TYPE_AXIS, left.getType());
        assertEquals(0, left.getIndex());
        assertHasModifier(left, GamepadModifier.GAMEPAD_MODIFIER_NEGATE);
        assertHasModifier(left, GamepadModifier.GAMEPAD_MODIFIER_CLAMP);

        GamepadMapEntry right = find(driver, Gamepad.GAMEPAD_LSTICK_RIGHT);
        assertEquals(GamepadType.GAMEPAD_TYPE_AXIS, right.getType());
        assertEquals(0, right.getIndex());
        assertHasModifier(right, GamepadModifier.GAMEPAD_MODIFIER_CLAMP);

        GamepadMapEntry trigger = find(driver, Gamepad.GAMEPAD_LTRIGGER);
        assertEquals(GamepadType.GAMEPAD_TYPE_AXIS, trigger.getType());
        assertEquals(2, trigger.getIndex());
        assertHasModifier(trigger, GamepadModifier.GAMEPAD_MODIFIER_SCALE);

        GamepadMapEntry dpadUp = find(driver, Gamepad.GAMEPAD_LPAD_UP);
        assertEquals(GamepadType.GAMEPAD_TYPE_HAT, dpadUp.getType());
        assertEquals(0, dpadUp.getIndex());
        assertEquals(1, dpadUp.getHatMask());
    }

    @Test
    public void testConvertSignedAxisBindings() throws Exception {
        String sdl = "03000000000000000000000000000000,Axis Dpad,dpdown:+a1,dpleft:-a0,dpright:+a0,dpup:-a1,platform:Linux,\n";

        GamepadMaps maps = parse(GamepadConverter.convertToDefoldFormat(sdl, "arm64-linux"));
        GamepadMap driver = maps.getDriver(0);

        GamepadMapEntry left = find(driver, Gamepad.GAMEPAD_LPAD_LEFT);
        assertEquals(GamepadType.GAMEPAD_TYPE_AXIS, left.getType());
        assertEquals(0, left.getIndex());
        assertHasModifier(left, GamepadModifier.GAMEPAD_MODIFIER_NEGATE);
        assertHasModifier(left, GamepadModifier.GAMEPAD_MODIFIER_CLAMP);

        GamepadMapEntry right = find(driver, Gamepad.GAMEPAD_LPAD_RIGHT);
        assertEquals(GamepadType.GAMEPAD_TYPE_AXIS, right.getType());
        assertEquals(0, right.getIndex());
        assertHasModifier(right, GamepadModifier.GAMEPAD_MODIFIER_CLAMP);
    }

    @Test
    public void testGamepadBuilderCombinesGamepadsAndGamepadDb() throws Exception {
        String gamepads = ""
                + "driver {\n"
                + "  device: \"Manual Pad\"\n"
                + "  platform: \"macos\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n"
                + "driver {\n"
                + "  device: \"Ignored Manual Pad\"\n"
                + "  platform: \"windows\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n";
        String gamepadDb = ""
                + "03000000000000000000000000000001,SDL Pad,a:b1,platform:Mac OS X,\n"
                + "03000000000000000000000000000002,Ignored SDL Pad,a:b2,platform:Linux,\n";

        addFile("/pad.gamepads", gamepads);
        addFile("/gamecontrollerdb.txt", gamepadDb);
        getProject().setOption("platform", "x86_64-macos");

        Project project = getProject();
        Task task = project.createGamepadTask(getFileSystem().get("/gamecontrollerdb.txt"), getFileSystem().get("/pad.gamepads"));
        assertEquals(2, task.getInputs().size());
        assertEquals("gamecontrollerdb.txt", task.input(0).getPath());
        assertEquals("pad.gamepads", task.input(1).getPath());

        task.getBuilder().build(task);
        GamepadMaps maps = GamepadMaps.parseFrom(task.output(0).getContent());

        assertEquals(2, maps.getDriverCount());
        assertEquals("SDL Pad", maps.getDriver(0).getDevice());
        assertEquals("Manual Pad", maps.getDriver(1).getDevice());
        assertEquals("macos", maps.getDriver(0).getPlatform());
        assertEquals("macos", maps.getDriver(1).getPlatform());
        assertEquals(1, find(maps.getDriver(0), Gamepad.GAMEPAD_RPAD_DOWN).getIndex());
    }

    private static GamepadMaps parse(String textFormat) throws Exception {
        GamepadMaps.Builder builder = GamepadMaps.newBuilder();
        TextFormat.merge(textFormat, builder);
        return builder.build();
    }

    private static GamepadMapEntry find(GamepadMap map, Gamepad input) {
        for (GamepadMapEntry entry : map.getMapList()) {
            if (entry.getInput() == input) {
                return entry;
            }
        }
        throw new AssertionError("Missing gamepad input: " + input);
    }

    private static void assertHasModifier(GamepadMapEntry entry, GamepadModifier modifier) {
        for (int i = 0; i < entry.getModCount(); i++) {
            if (entry.getMod(i).getMod() == modifier) {
                return;
            }
        }
        assertTrue("Missing modifier " + modifier + " for " + entry.getInput(), false);
    }
}
