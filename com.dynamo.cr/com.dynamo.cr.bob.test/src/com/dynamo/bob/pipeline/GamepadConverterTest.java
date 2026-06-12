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
import static org.junit.Assert.assertFalse;
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
        assertFalse(driver.hasDeadZone());
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
        assertEquals(4, trigger.getIndex());
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
    public void testConvertSignedStickAxisBindings() throws Exception {
        String sdl = "030000005e0400008e02000014010000,Xbox 360 Controller,leftx:-a0,leftx:+a0,lefty:+a1,lefty:-a1,platform:Linux,\n";

        GamepadMaps maps = parse(GamepadConverter.convertToDefoldFormat(sdl, "arm64-linux"));
        GamepadMap driver = maps.getDriver(0);

        assertEquals(4, driver.getMapCount());

        GamepadMapEntry left = find(driver, Gamepad.GAMEPAD_LSTICK_LEFT);
        assertEquals(GamepadType.GAMEPAD_TYPE_AXIS, left.getType());
        assertEquals(0, left.getIndex());
        assertHasModifier(left, GamepadModifier.GAMEPAD_MODIFIER_NEGATE);
        assertHasModifier(left, GamepadModifier.GAMEPAD_MODIFIER_CLAMP);

        GamepadMapEntry right = find(driver, Gamepad.GAMEPAD_LSTICK_RIGHT);
        assertEquals(GamepadType.GAMEPAD_TYPE_AXIS, right.getType());
        assertEquals(0, right.getIndex());
        assertHasModifier(right, GamepadModifier.GAMEPAD_MODIFIER_CLAMP);
        assertMissingModifier(right, GamepadModifier.GAMEPAD_MODIFIER_NEGATE);

        GamepadMapEntry down = find(driver, Gamepad.GAMEPAD_LSTICK_DOWN);
        assertEquals(GamepadType.GAMEPAD_TYPE_AXIS, down.getType());
        assertEquals(1, down.getIndex());
        assertHasModifier(down, GamepadModifier.GAMEPAD_MODIFIER_CLAMP);
        assertMissingModifier(down, GamepadModifier.GAMEPAD_MODIFIER_NEGATE);

        GamepadMapEntry up = find(driver, Gamepad.GAMEPAD_LSTICK_UP);
        assertEquals(GamepadType.GAMEPAD_TYPE_AXIS, up.getType());
        assertEquals(1, up.getIndex());
        assertHasModifier(up, GamepadModifier.GAMEPAD_MODIFIER_NEGATE);
        assertHasModifier(up, GamepadModifier.GAMEPAD_MODIFIER_CLAMP);
    }

    @Test
    public void testConvertSdlMappingKeepsRawPacketLayoutOnLinux() throws Exception {
        String sdl = "03000000000000000000000000000001,Test Pad,a:b1,lefttrigger:a2,platform:Linux,\n";

        GamepadMaps maps = parse(GamepadConverter.convertToDefoldFormat(sdl, "arm64-linux"));
        GamepadMap driver = maps.getDriver(0);

        assertEquals("Test Pad", driver.getDevice());
        assertEquals("linux", driver.getPlatform());
        assertEquals(1, find(driver, Gamepad.GAMEPAD_RPAD_DOWN).getIndex());
        assertEquals(2, find(driver, Gamepad.GAMEPAD_LTRIGGER).getIndex());
    }

    @Test
    public void testConvertSdlMacosMappingInfersCanonicalOptionalButtonSlots() throws Exception {
        String sdl = "03000000000000000000000000000001,Simple Pad,a:b1,leftshoulder:b4,rightshoulder:b5,lefttrigger:a2,platform:Mac OS X,\n";

        GamepadMaps maps = parse(GamepadConverter.convertToDefoldFormat(sdl, "x86_64-macos"));
        GamepadMap driver = maps.getDriver(0);

        assertEquals("Simple Pad", driver.getDevice());
        assertEquals("macos", driver.getPlatform());
        assertEquals(0, find(driver, Gamepad.GAMEPAD_RPAD_DOWN).getIndex());
        assertEquals(4, find(driver, Gamepad.GAMEPAD_LSHOULDER).getIndex());
        assertEquals(5, find(driver, Gamepad.GAMEPAD_RSHOULDER).getIndex());
        assertEquals(4, find(driver, Gamepad.GAMEPAD_LTRIGGER).getIndex());
    }

    @Test
    public void testConvertCanonicalSdlXbox360MacosMapping() throws Exception {
        String sdl = "030000005e0400008e02000014010000,Xbox 360 Controller,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b10,start:b5,leftstick:b6,rightstick:b7,leftshoulder:b8,rightshoulder:b9,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,platform:Mac OS X,\n";

        GamepadMaps maps = parse(GamepadConverter.convertToDefoldFormat(sdl, "x86_64-macos"));
        GamepadMap driver = maps.getDriver(0);

        assertEquals("Xbox 360 Controller", driver.getDevice());
        assertEquals("macos", driver.getPlatform());

        assertEquals(4, find(driver, Gamepad.GAMEPAD_BACK).getIndex());
        assertEquals(5, find(driver, Gamepad.GAMEPAD_START).getIndex());
        assertEquals(8, find(driver, Gamepad.GAMEPAD_LSHOULDER).getIndex());
        assertEquals(9, find(driver, Gamepad.GAMEPAD_RSHOULDER).getIndex());
        assertEquals(10, find(driver, Gamepad.GAMEPAD_GUIDE).getIndex());

        GamepadMapEntry dpadUp = find(driver, Gamepad.GAMEPAD_LPAD_UP);
        assertEquals(GamepadType.GAMEPAD_TYPE_HAT, dpadUp.getType());
        assertEquals(0, dpadUp.getIndex());
        assertEquals(1, dpadUp.getHatMask());

        assertEquals(2, find(driver, Gamepad.GAMEPAD_RSTICK_LEFT).getIndex());
        assertEquals(4, find(driver, Gamepad.GAMEPAD_LTRIGGER).getIndex());
        assertEquals(5, find(driver, Gamepad.GAMEPAD_RTRIGGER).getIndex());

        assertHasModifier(find(driver, Gamepad.GAMEPAD_LSTICK_UP), GamepadModifier.GAMEPAD_MODIFIER_NEGATE);
        assertHasModifier(find(driver, Gamepad.GAMEPAD_RSTICK_UP), GamepadModifier.GAMEPAD_MODIFIER_NEGATE);
        assertHasModifier(find(driver, Gamepad.GAMEPAD_LTRIGGER), GamepadModifier.GAMEPAD_MODIFIER_SCALE);
        assertHasModifier(find(driver, Gamepad.GAMEPAD_RTRIGGER), GamepadModifier.GAMEPAD_MODIFIER_SCALE);
    }

    @Test
    public void testConvertUpstreamSdlDualSenseBluetoothMacosMappingToCanonicalPacketLayout() throws Exception {
        String sdl = "050000004c050000e60c000000010000,PS5 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,touchpad:b13,x:b0,y:b3,platform:Mac OS X,\n";

        GamepadMaps maps = parse(GamepadConverter.convertToDefoldFormat(sdl, "x86_64-macos"));
        GamepadMap driver = maps.getDriver(0);

        assertEquals("PS5 Controller", driver.getDevice());
        assertEquals("macos", driver.getPlatform());

        assertEquals(0, find(driver, Gamepad.GAMEPAD_RPAD_DOWN).getIndex());
        assertEquals(1, find(driver, Gamepad.GAMEPAD_RPAD_RIGHT).getIndex());
        assertEquals(2, find(driver, Gamepad.GAMEPAD_RPAD_LEFT).getIndex());
        assertEquals(3, find(driver, Gamepad.GAMEPAD_RPAD_UP).getIndex());
        assertEquals(4, find(driver, Gamepad.GAMEPAD_BACK).getIndex());
        assertEquals(5, find(driver, Gamepad.GAMEPAD_START).getIndex());
        assertEquals(6, find(driver, Gamepad.GAMEPAD_LSTICK_CLICK).getIndex());
        assertEquals(7, find(driver, Gamepad.GAMEPAD_RSTICK_CLICK).getIndex());
        assertEquals(8, find(driver, Gamepad.GAMEPAD_LSHOULDER).getIndex());
        assertEquals(9, find(driver, Gamepad.GAMEPAD_RSHOULDER).getIndex());
        assertEquals(10, find(driver, Gamepad.GAMEPAD_GUIDE).getIndex());

        GamepadMapEntry dpadUp = find(driver, Gamepad.GAMEPAD_LPAD_UP);
        assertEquals(GamepadType.GAMEPAD_TYPE_HAT, dpadUp.getType());
        assertEquals(0, dpadUp.getIndex());
        assertEquals(1, dpadUp.getHatMask());

        assertEquals(2, find(driver, Gamepad.GAMEPAD_RSTICK_LEFT).getIndex());
        assertEquals(4, find(driver, Gamepad.GAMEPAD_LTRIGGER).getIndex());
        assertEquals(5, find(driver, Gamepad.GAMEPAD_RTRIGGER).getIndex());

        assertHasModifier(find(driver, Gamepad.GAMEPAD_LSTICK_UP), GamepadModifier.GAMEPAD_MODIFIER_NEGATE);
        assertHasModifier(find(driver, Gamepad.GAMEPAD_RSTICK_UP), GamepadModifier.GAMEPAD_MODIFIER_NEGATE);
        assertHasModifier(find(driver, Gamepad.GAMEPAD_LTRIGGER), GamepadModifier.GAMEPAD_MODIFIER_SCALE);
        assertHasModifier(find(driver, Gamepad.GAMEPAD_RTRIGGER), GamepadModifier.GAMEPAD_MODIFIER_SCALE);
    }

    @Test
    public void testGamepadBuilderCombinesGamepadsAndGamepadDb() throws Exception {
        String gamepads = ""
                + "driver {\n"
                + "  device: \"Manual Pad\"\n"
                + "  platform: \"osx\"\n"
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
        project.getProjectProperties().putStringValue("input", "gamepad_deadzone", "0.35");
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
        assertEquals(0, find(maps.getDriver(0), Gamepad.GAMEPAD_RPAD_DOWN).getIndex());
        assertFalse(maps.getDriver(0).hasDeadZone());
        assertEquals(0.2f, maps.getDriver(1).getDeadZone(), 0.0f);
    }

    @Test
    public void testGamepadBuilderWithOnlyGamepadDb() throws Exception {
        String gamepadDb = ""
                + "03000000000000000000000000000001,SDL Pad,a:b1,platform:Mac OS X,\n"
                + "03000000000000000000000000000002,Ignored SDL Pad,a:b2,platform:Linux,\n";

        addFile("/gamecontrollerdb.txt", gamepadDb);
        getProject().setOption("platform", "x86_64-macos");

        Project project = getProject();
        Task task = project.createGamepadTask(getFileSystem().get("/gamecontrollerdb.txt"), null);
        assertEquals(1, task.getInputs().size());
        assertEquals("gamecontrollerdb.txt", task.input(0).getPath());
        assertEquals("build/gamecontrollerdb.gamepadsc", task.output(0).getPath());

        task.getBuilder().build(task);
        GamepadMaps maps = GamepadMaps.parseFrom(task.output(0).getContent());

        assertEquals(1, maps.getDriverCount());
        assertEquals("SDL Pad", maps.getDriver(0).getDevice());
        assertEquals("macos", maps.getDriver(0).getPlatform());
        assertEquals(0, find(maps.getDriver(0), Gamepad.GAMEPAD_RPAD_DOWN).getIndex());
        assertFalse(maps.getDriver(0).hasDeadZone());
    }

    @Test
    public void testGamepadBuilderWithOnlyGamepads() throws Exception {
        String gamepads = ""
                + "driver {\n"
                + "  device: \"Manual Pad\"\n"
                + "  platform: \"osx\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n"
                + "driver {\n"
                + "  device: \"Ignored Manual Pad\"\n"
                + "  platform: \"windows\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n";

        addFile("/pad.gamepads", gamepads);
        getProject().setOption("platform", "x86_64-macos");

        Project project = getProject();
        Task task = project.createGamepadTask(null, getFileSystem().get("/pad.gamepads"));
        assertEquals(1, task.getInputs().size());
        assertEquals("pad.gamepads", task.input(0).getPath());
        assertEquals("build/pad.gamepadsc", task.output(0).getPath());

        task.getBuilder().build(task);
        GamepadMaps maps = GamepadMaps.parseFrom(task.output(0).getContent());

        assertEquals(1, maps.getDriverCount());
        assertEquals("Manual Pad", maps.getDriver(0).getDevice());
        assertEquals("macos", maps.getDriver(0).getPlatform());
        assertEquals(0, find(maps.getDriver(0), Gamepad.GAMEPAD_RPAD_DOWN).getIndex());
        assertEquals(0.2f, maps.getDriver(0).getDeadZone(), 0.0f);
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

    private static void assertMissingModifier(GamepadMapEntry entry, GamepadModifier modifier) {
        for (int i = 0; i < entry.getModCount(); i++) {
            assertFalse("Unexpected modifier " + modifier + " for " + entry.getInput(), entry.getMod(i).getMod() == modifier);
        }
    }
}
