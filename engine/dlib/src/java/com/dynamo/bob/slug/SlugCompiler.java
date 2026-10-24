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

package com.dynamo.bob.slug;

import static java.lang.foreign.ValueLayout.JAVA_FLOAT;
import static java.lang.foreign.ValueLayout.JAVA_INT;

import com.dynamo.bob.slug.generated.SlugFFM;
import com.dynamo.bob.slug.generated.SlugImage;
import com.dynamo.bob.slug.generated.SlugPath;
import com.dynamo.bob.slug.generated.SlugTexture;
import java.io.File;
import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/** Java 25 bridge to dlib's font-independent Slug compiler. */
public final class SlugCompiler {
    static {
        boolean loaded = false;
        try {
            Class<?> bob = SlugCompiler.class.getClassLoader().loadClass("com.dynamo.bob.Bob");
            Method getSharedLib = bob.getMethod("getSharedLib", String.class);
            File library = (File)getSharedLib.invoke(null, "dlib_shared");
            System.load(library.getAbsolutePath());
            loaded = true;
        } catch (ReflectiveOperationException | RuntimeException | LinkageError ignored) {
        }
        if (!loaded)
            System.loadLibrary("dlib_shared");
    }

    private SlugCompiler() {}

    /** Java-owned snapshot of a complete numeric texture. */
    public static final class Texture {
        public final ByteBuffer pixels;
        public final int width;
        public final int height;
        public final int channels;

        private Texture(MemorySegment source) {
            width = SlugTexture.m_Width(source);
            height = SlugTexture.m_Height(source);
            channels = SlugTexture.m_Channels(source);
            int size = SlugTexture.m_PixelCount(source);
            pixels = ByteBuffer.allocateDirect(size).order(ByteOrder.LITTLE_ENDIAN);
            pixels.put(SlugTexture.m_Pixels(source).reinterpret(size).asByteBuffer()).flip();
        }
    }

    /** Parsed vector geometry, independent of SVG and font decoding. */
    public record Path(float[] commands, boolean evenOdd, int paintType, int spread,
                       float[] parameters, float[] paintTransform, float[] stops) {}
    public record Image(float width, float height, Path[] paths) {}
    public record Atlas(Texture curves, Texture bands, Texture paints, int[] imagePaints) {}

    /** Compiles one numeric atlas. The returned buffers are Java-owned snapshots. */
    public static Atlas compileAtlas(Image[] images) {
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment nativeImages = SlugImage.allocateArray(images.length, arena);
            for (int i = 0; i < images.length; ++i) {
                Image image = images[i];
                MemorySegment ni = SlugImage.asSlice(nativeImages, i);
                MemorySegment paths = SlugPath.allocateArray(image.paths.length, arena);
                SlugImage.m_Width(ni, image.width);
                SlugImage.m_Height(ni, image.height);
                SlugImage.m_Paths(ni, paths);
                SlugImage.m_PathCount(ni, image.paths.length);
                for (int j = 0; j < image.paths.length; ++j) {
                    Path path = image.paths[j];
                    if (path.commands.length % 7 != 0 || path.stops.length % 5 != 0 ||
                            path.parameters.length != 6 || path.paintTransform.length != 6)
                        throw new IllegalArgumentException("Invalid vector path array dimensions");
                    MemorySegment np = SlugPath.asSlice(paths, j);
                    SlugPath.m_Commands(np, arena.allocateFrom(JAVA_FLOAT, path.commands));
                    SlugPath.m_CommandCount(np, path.commands.length / 7);
                    SlugPath.m_EvenOdd(np, path.evenOdd ? 1 : 0);
                    SlugPath.m_PaintType(np, path.paintType);
                    SlugPath.m_Spread(np, path.spread);
                    SlugPath.m_Parameters(np, MemorySegment.ofArray(path.parameters));
                    SlugPath.m_PaintTransform(np, MemorySegment.ofArray(path.paintTransform));
                    SlugPath.m_Stops(np, arena.allocateFrom(JAVA_FLOAT, path.stops));
                    SlugPath.m_StopCount(np, path.stops.length / 5);
                }
            }
            MemorySegment indices = arena.allocate(JAVA_INT, images.length);
            MemorySegment curves = SlugTexture.allocate(arena);
            MemorySegment bands = SlugTexture.allocate(arena);
            MemorySegment paints = SlugTexture.allocate(arena);
            try {
                int result = SlugFFM.SlugCompileAtlas(nativeImages, images.length, curves, bands, paints, indices);
                if (result == SlugFFM.SLUG_RESULT_CAPACITY())
                    throw new IllegalArgumentException("Vector atlas capacity exceeded (4096 x 256 numeric texels, " +
                            "1024 curves per path or 1024 gradient stops). Split the atlas or simplify the SVG paths.");
                if (result != SlugFFM.SLUG_RESULT_OK())
                    throw new IllegalStateException("Vector atlas compilation failed: " + result);
                return new Atlas(new Texture(curves), new Texture(bands), new Texture(paints), indices.toArray(JAVA_INT));
            } finally {
                SlugFFM.SlugFreeTexture(curves);
                SlugFFM.SlugFreeTexture(bands);
                SlugFFM.SlugFreeTexture(paints);
            }
        }
    }

}
