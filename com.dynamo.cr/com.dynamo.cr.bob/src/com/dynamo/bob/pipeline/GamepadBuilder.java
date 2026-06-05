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

import java.io.ByteArrayOutputStream;
import java.io.IOException;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.input.proto.Input.GamepadMap;
import com.dynamo.input.proto.Input.GamepadMaps;

/**
 * Builder that combines Defold .gamepads and an optional SDL gamecontrollerdb.txt into binary .gamepadsc.
 */
@BuilderParams(name = "Gamepad", inExts = {}, outExt = ".gamepadsc", paramsForSignature = {"platform"})
public class GamepadBuilder extends Builder {

    private static final String EXT_SDL = ".txt";
    private static final String EXT_GAMEPADS = ".gamepads";

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        return Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()))
                .build();
    }

    @Override
    public void build(Task task) throws IOException, CompileExceptionError {
        IResource output = task.output(0);

        String platform = project.getPlatform().getPair();

        GamepadMaps.Builder gamepadMapsBuilder = GamepadMaps.newBuilder();

        for (IResource input : task.getInputs()) {
            String content = new String(input.getContent(), java.nio.charset.StandardCharsets.UTF_8);
            if (input.getPath().endsWith(EXT_SDL)) {
                String defoldTextFormat = GamepadConverter.convertToDefoldFormat(content, platform);
                parseAsGamepadMaps(input, defoldTextFormat, gamepadMapsBuilder);
            } else if (input.getPath().endsWith(EXT_GAMEPADS)) {
                parseAsGamepadMaps(input, content, gamepadMapsBuilder);
            }
        }

        GamepadMaps gamepadMaps = filterPlatform(gamepadMapsBuilder.buildPartial(), platform);
        ByteArrayOutputStream out = new ByteArrayOutputStream(4 * 1024);
        gamepadMaps.writeTo(out);
        out.close();
        output.setContent(out.toByteArray());
    }

    private GamepadMaps filterPlatform(GamepadMaps gamepadMaps, String platform) {
        String normalizedPlatform = GamepadConverter.normalizePlatform(platform);
        GamepadMaps.Builder filtered = GamepadMaps.newBuilder();
        for (GamepadMap driver : gamepadMaps.getDriverList()) {
            if (GamepadConverter.platformMatches(driver.getPlatform(), normalizedPlatform)) {
                filtered.addDriver(GamepadMap.newBuilder(driver)
                        .setPlatform(normalizedPlatform)
                        .build());
            }
        }
        return filtered.build();
    }

    /**
     * Parse Defold TextFormat string into a GamepadMaps protobuf message.
     */
    private void parseAsGamepadMaps(IResource input, String textFormat, GamepadMaps.Builder builder) throws IOException, CompileExceptionError {
        try {
            com.google.protobuf.TextFormat.merge(textFormat, builder);
        } catch (com.google.protobuf.TextFormat.ParseException e) {
            throw new CompileExceptionError(input, 0, "Failed to parse .gamepads: " + e.getMessage(), e);
        }
    }
}
