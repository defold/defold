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

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.input.proto.Input.GamepadMap;
import com.dynamo.input.proto.Input.GamepadMapRuntime;
import com.dynamo.input.proto.Input.GamepadMaps;
import com.dynamo.input.proto.Input.GamepadMapsRuntime;

/**
 * Builder that combines Defold .gamepads and an optional SDL gamecontrollerdb.txt into binary .gamepadsc.
 */
@BuilderParams(name = "Gamepad", inExts = {".gamepads"}, outExt = ".gamepadsc", paramsForSignature = {"platform"})
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
        String[] inputPaths = new String[task.getInputs().size()];
        byte[][] inputContents = new byte[task.getInputs().size()][];

        for (int i = 0; i < task.getInputs().size(); ++i) {
            IResource input = task.getInputs().get(i);
            inputPaths[i] = input.getPath();
            inputContents[i] = input.getContent();
        }

        output.setContent(compile(inputPaths, inputContents, platform));
    }

    /**
     * Compile optional .gamepads and gamecontrollerdb.txt bytes into binary GamepadMapsRuntime output.
     */
    public static byte[] compile(String gamepadsPath, byte[] gamepadsContent, String gamecontrollerdbPath, byte[] gamecontrollerdbContent, String platform) throws CompileExceptionError {
        List<String> paths = new ArrayList<String>();
        List<byte[]> contents = new ArrayList<byte[]>();

        // Keep database mappings before .gamepads mappings to match Bob project builds.
        if (gamecontrollerdbPath != null && gamecontrollerdbContent != null) {
            paths.add(gamecontrollerdbPath);
            contents.add(gamecontrollerdbContent);
        }

        if (gamepadsPath != null && gamepadsContent != null) {
            paths.add(gamepadsPath);
            contents.add(gamepadsContent);
        }

        return compile(paths.toArray(new String[paths.size()]), contents.toArray(new byte[contents.size()][]), platform);
    }

    /**
     * Compile input bytes into binary GamepadMapsRuntime output, preserving input order.
     */
    public static byte[] compile(String[] inputPaths, byte[][] inputContents, String platform) throws CompileExceptionError {
        if (inputPaths.length != inputContents.length) {
            throw new CompileExceptionError("Gamepad input paths and contents must have the same length.");
        }

        return buildGamepadMapsRuntime(inputPaths, inputContents, platform).toByteArray();
    }

    private static GamepadMapsRuntime buildGamepadMapsRuntime(String[] inputPaths, byte[][] inputContents, String platform) throws CompileExceptionError {
        GamepadMapsRuntime.Builder gamepadMapsBuilder = GamepadMapsRuntime.newBuilder();

        for (int i = 0; i < inputPaths.length; ++i) {
            String inputPath = inputPaths[i];
            String content = new String(inputContents[i], StandardCharsets.UTF_8);
            if (inputPath.endsWith(EXT_SDL)) {
                String runtimeTextFormat = GamepadConverter.convertToRuntimeFormat(content, platform);
                parseAsGamepadMapsRuntime(inputPath, runtimeTextFormat, gamepadMapsBuilder);
            } else if (inputPath.endsWith(EXT_GAMEPADS)) {
                GamepadMaps.Builder editorGamepadMapsBuilder = GamepadMaps.newBuilder();
                parseAsGamepadMaps(inputPath, content, editorGamepadMapsBuilder);
                GamepadMaps filteredGamepadMaps = filterPlatform(editorGamepadMapsBuilder.buildPartial(), platform);
                gamepadMapsBuilder.addAllMappings(toRuntime(filteredGamepadMaps).getMappingsList());
            }
        }

        return gamepadMapsBuilder.build();
    }

    private static GamepadMaps filterPlatform(GamepadMaps gamepadMaps, String platform) {
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

    private static GamepadMapsRuntime toRuntime(GamepadMaps gamepadMaps) {
        GamepadMapsRuntime.Builder runtime = GamepadMapsRuntime.newBuilder();
        for (GamepadMap driver : gamepadMaps.getDriverList()) {
            GamepadMapRuntime.Builder mapping = GamepadMapRuntime.newBuilder()
                    .setDevice(driver.getDevice())
                    .addAllMap(driver.getMapList());
            if (driver.hasDeadZone()) {
                mapping.setDeadZone(driver.getDeadZone());
            }
            runtime.addMappings(mapping);
        }
        return runtime.build();
    }

    /**
     * Parse Defold TextFormat string into a GamepadMaps protobuf message.
     */
    private static void parseAsGamepadMaps(String inputPath, String textFormat, GamepadMaps.Builder builder) throws CompileExceptionError {
        try {
            com.google.protobuf.TextFormat.merge(textFormat, builder);
        } catch (com.google.protobuf.TextFormat.ParseException e) {
            throw new CompileExceptionError(inputPath + ": Failed to parse .gamepads: " + e.getMessage(), e);
        }
    }

    /**
     * Parse Defold runtime TextFormat string into a GamepadMapsRuntime protobuf message.
     */
    private static void parseAsGamepadMapsRuntime(String inputPath, String textFormat, GamepadMapsRuntime.Builder builder) throws CompileExceptionError {
        try {
            com.google.protobuf.TextFormat.merge(textFormat, builder);
        } catch (com.google.protobuf.TextFormat.ParseException e) {
            throw new CompileExceptionError(inputPath + ": Failed to parse gamecontrollerdb.txt: " + e.getMessage(), e);
        }
    }

    // Running standalone:
    // java -classpath $DYNAMO_HOME/share/java/bob-light.jar com.dynamo.bob.pipeline.GamepadBuilder <path-in.gamepads> [<path-in.gamecontrollerdb.txt>...] <path-out.gamepadsc> <platform>
    public static void main(String[] args) throws IOException, CompileExceptionError {
        System.setProperty("java.awt.headless", "true");

        if (args.length < 3) {
            System.err.println("Usage: GamepadBuilder <path-in.gamepads> [<path-in.gamecontrollerdb.txt>...] <path-out.gamepadsc> <platform>");
            return;
        }

        String outputPath = args[args.length - 2];
        String platform = args[args.length - 1];

        String[] inputPaths = new String[args.length - 2];
        byte[][] inputContents = new byte[args.length - 2][];
        for (int i = 0; i < args.length - 2; ++i) {
            inputPaths[i] = args[i];
            inputContents[i] = Files.readAllBytes(Paths.get(inputPaths[i]));
        }

        Files.write(Paths.get(outputPath), compile(inputPaths, inputContents, platform));
    }
}
