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
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

import org.junit.Test;

import com.dynamo.input.proto.Input.GamepadMapRuntime;
import com.dynamo.input.proto.Input.GamepadMapsRuntime;

public class GamepadBuilderTest {

    private static File repoFile(String path) throws Exception {
        return new File(System.getProperty("user.dir"), path).getCanonicalFile();
    }

    private static boolean hasMapping(GamepadMapsRuntime maps, String device, boolean hasRawMapping) {
        for (GamepadMapRuntime mapping : maps.getMappingsList()) {
            if (mapping.getDevice().equals(device) && mapping.hasRawMapping() == hasRawMapping) {
                return true;
            }
        }
        return false;
    }

    private static String firstMappingWithoutControls(GamepadMapsRuntime maps) {
        for (GamepadMapRuntime mapping : maps.getMappingsList()) {
            if (mapping.getMapCount() == 0) {
                return mapping.getDevice();
            }
        }
        return null;
    }

    @Test
    public void testMainBuildsDefaultGamepadsAsRuntimeFormat() throws Exception {
        File defaultGamepads = repoFile("../../engine/engine/content/builtins/input/default.gamepads");
        File gamecontrollerdb = repoFile("../../engine/engine/content/builtins/input/gamecontrollerdb.txt");
        File output = File.createTempFile("default-gamepads", ".gamepadsc");
        output.deleteOnExit();

        try {
            GamepadBuilder.main(new String[] {
                    defaultGamepads.getAbsolutePath(),
                    gamecontrollerdb.getAbsolutePath(),
                    output.getAbsolutePath(),
                    "x86_64-macos"
            });

            GamepadMapsRuntime maps = GamepadMapsRuntime.parseFrom(Files.readAllBytes(output.toPath()));
            assertTrue(maps.getMappingsCount() > 0);
            assertNull(firstMappingWithoutControls(maps));
            assertTrue("Expected a converted mapping from default.gamepads without a GUID.",
                    hasMapping(maps, "Wireless Controller", false));
            assertTrue("Expected a converted mapping from gamecontrollerdb.txt with a GUID.",
                    hasMapping(maps, "Nintendo Switch Pro Controller", true));
        } finally {
            output.delete();
        }
    }

    @Test
    public void testMainBuildsXboxDefaultGamepads() throws Exception {
        File defaultGamepads = repoFile("../../engine/engine/content/builtins/input/default.gamepads");
        File gamecontrollerdb = repoFile("../../engine/engine/content/builtins/input/gamecontrollerdb.txt");
        File output = File.createTempFile("default-gamepads-xbox", ".gamepadsc");
        output.deleteOnExit();

        try {
            GamepadBuilder.main(new String[] {
                    defaultGamepads.getAbsolutePath(),
                    gamecontrollerdb.getAbsolutePath(),
                    output.getAbsolutePath(),
                    "x86_64-xbone"
            });

            GamepadMapsRuntime maps = GamepadMapsRuntime.parseFrom(Files.readAllBytes(output.toPath()));
            assertEquals(1, maps.getMappingsCount());
            assertNull(firstMappingWithoutControls(maps));
            assertTrue("Expected the legacy default.gamepads Xbox mapping.",
                    hasMapping(maps, "Xbox One Controller", false));
        } finally {
            output.delete();
        }
    }

    @Test
    public void testMainCombinesInputsAndFiltersByTargetPlatform() throws Exception {
        File gamepads = File.createTempFile("platform-filter", ".gamepads");
        File gamecontrollerdb = File.createTempFile("platform-filter", ".txt");
        gamepads.deleteOnExit();
        gamecontrollerdb.deleteOnExit();

        try {
            Files.write(gamepads.toPath(), (""
                    + "driver {\n"
                    + "  device: \"Manual Mac Pad\"\n"
                    + "  platform: \"macos\"\n"
                    + "  dead_zone: 0.2\n"
                    + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                    + "}\n"
                    + "driver {\n"
                    + "  device: \"Manual Linux Pad\"\n"
                    + "  platform: \"linux\"\n"
                    + "  dead_zone: 0.2\n"
                    + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                    + "}\n"
                    + "driver {\n"
                    + "  device: \"Manual Windows Pad\"\n"
                    + "  platform: \"windows\"\n"
                    + "  dead_zone: 0.2\n"
                    + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                    + "}\n").getBytes(StandardCharsets.UTF_8));

            Files.write(gamecontrollerdb.toPath(), (""
                    + "03000000000000000000000000000001,SDL Mac Pad,a:b0,platform:Mac OS X,\n"
                    + "03000000000000000000000000000002,SDL Linux Pad,a:b0,platform:Linux,\n"
                    + "03000000000000000000000000000003,SDL Windows Pad,a:b0,platform:Windows,\n").getBytes(StandardCharsets.UTF_8));

            byte[] outputBytes = GamepadBuilder.compile(
                    gamepads.getAbsolutePath(),
                    Files.readAllBytes(gamepads.toPath()),
                    gamecontrollerdb.getAbsolutePath(),
                    Files.readAllBytes(gamecontrollerdb.toPath()),
                    "x86_64-macos");

            GamepadMapsRuntime maps = GamepadMapsRuntime.parseFrom(outputBytes);
            assertEquals(2, maps.getMappingsCount());
            assertTrue(hasMapping(maps, "Manual Mac Pad", false));
            assertTrue(hasMapping(maps, "SDL Mac Pad", true));
            assertFalse(hasMapping(maps, "Manual Linux Pad", false));
            assertFalse(hasMapping(maps, "Manual Windows Pad", false));
            assertFalse(hasMapping(maps, "SDL Linux Pad", true));
            assertFalse(hasMapping(maps, "SDL Windows Pad", true));
            assertNull(firstMappingWithoutControls(maps));
        } finally {
            gamepads.delete();
            gamecontrollerdb.delete();
        }
    }

}
