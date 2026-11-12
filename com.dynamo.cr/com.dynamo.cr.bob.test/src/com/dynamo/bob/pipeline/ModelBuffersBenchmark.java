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

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Locale;

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.rig.proto.Rig;

/** Production native import/packing path for benchmark_model_buffers.cpp. */
public final class ModelBuffersBenchmark {
    private static volatile long checksum;

    public static void main(String[] args) throws Exception {
        if (args.length < 3) {
            throw new IllegalArgumentException("usage: ModelBuffersBenchmark manifest.tsv warmups samples [output-directory]");
        }
        System.setProperty("java.awt.headless", "true");
        List<String[]> inputs = Files.readAllLines(Path.of(args[0]), StandardCharsets.UTF_8).stream()
                .map(line -> line.split("\t")).toList();
        int warmups = Integer.parseInt(args[1]);
        int samples = Integer.parseInt(args[2]);

        // Load native libraries and generated protobuf classes before timing.
        Class.forName(ModelImporterJni.class.getName());
        Rig.MeshSet.getDefaultInstance();
        System.out.println("sample\tmodel\tload_us\tprepare_us\tpack_us\tbytes\tranges");
        for (int sample = -warmups; sample < samples; ++sample) {
            for (int i = 0; i < inputs.size(); ++i) {
                String[] input = inputs.get(i);
                String path = input[1];
                int selection = Integer.parseInt(input[2]);
                Modelimporter.Options options = new Modelimporter.Options();
                options.packModelBuffers = true;
                options.onlyModelMetadata = true;
                options.skipImageData = true;
                long start = System.nanoTime();
                Modelimporter.Scene scene = ModelUtil.loadScene(Files.readAllBytes(Path.of(path)), path,
                        options, new ModelImporterJni.FileDataResolver());
                ModelUtil.releaseSceneBuffers(scene);
                long loaded = System.nanoTime();

                Rig.MeshSet.Builder meshSet = Rig.MeshSet.newBuilder();
                ModelUtil.loadModelMetadata(scene, meshSet, 1024, 1024);
                Rig.MeshSet selected = ModelBuffers.selectMeshes(meshSet.build(), selection);
                long prepared = System.nanoTime();
                ModelBuffers.Compiled compiled = ModelBuffers.compile(selected, scene.modelBuffers);
                byte[] output = compiled.data();
                // Bob content-addresses the complete native-generated input.
                checksum = MurmurHash.hash64(output, output.length);
                long packed = System.nanoTime();

                // Consume every output outside timing, including warmup passes.
                checksum = MurmurHash.hash64(output, output.length);

                if (sample >= 0) {
                    System.out.printf(Locale.ROOT, "%d\t%d\t%.3f\t%.3f\t%.3f\t%d\t%d%n", sample, i,
                            (loaded - start) / 1000.0, (prepared - loaded) / 1000.0,
                            (packed - prepared) / 1000.0, output.length, java.nio.ByteBuffer.wrap(output).order(java.nio.ByteOrder.LITTLE_ENDIAN).getInt(16));
                }
                if (args.length > 3 && sample == 0) {
                    Files.write(Path.of(args[3], i + ".modelbuffersc"), output);
                }
                ModelUtil.unloadScene(scene);
            }
        }
    }
}
