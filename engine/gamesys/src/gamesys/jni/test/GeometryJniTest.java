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

package com.defold.engine;

public final class GeometryJniTest {
    private static void Check(boolean condition) {
        if (!condition)
            throw new AssertionError();
    }

    private static void Near(float expected, float actual) {
        if (!(Math.abs(expected - actual) < 0.00001f))
            throw new AssertionError("Expected " + expected + ", got " + actual);
    }

    private static Geometry.Slice9Params Params() {
        Geometry.Slice9Params params = new Geometry.Slice9Params();
        params.width = 32.5f;
        params.height = 24.25f;
        params.slice9 = new float[] {2, 3, 4, 5};
        params.pivotX = 0.75f;
        params.pivotY = 0.25f;
        return params;
    }

    // Verifies every parameter survives JNI conversion, including the native bitfields.
    private static void Positions() {
        Geometry.Slice9Params params = Params();
        float[][] xs = {{-24.375f, -22.375f, 4.125f, 8.125f}, {-24.375f, -20.375f, 6.125f, 8.125f}};
        float[][] ys = {{-6.0625f, -1.0625f, 15.1875f, 18.1875f}, {-6.0625f, -3.0625f, 13.1875f, 18.1875f}};
        for (byte flip = 0; flip < 4; ++flip) {
            params.flipHorizontal = (byte)(flip & 1);
            params.flipVertical = (byte)((flip >> 1) & 1);
            float[] positions = GeometryJni.GenerateSlice9Positions(params);
            Check(positions.length == 32);
            for (int i = 0; i < 16; ++i) {
                Near(xs[params.flipHorizontal][i % 4], positions[i * 2] * params.width);
                Near(ys[params.flipVertical][i / 4], positions[i * 2 + 1] * params.height);
            }
        }
        params.width = 0;
        params.height = 0;
        for (float value : GeometryJni.GenerateSlice9Positions(params))
            Check(Float.isFinite(value));
    }

    // Verifies packed rotations, all flips and atlas dimensions cross JNI unchanged.
    private static void TexCoords() {
        float[][] corners = {
            {0.125f, 0.25f, 0.125f, 0.75f, 0.625f, 0.75f, 0.625f, 0.25f},
            {0.125f, 0.75f, 0.625f, 0.75f, 0.625f, 0.25f, 0.125f, 0.25f}
        };
        float[][] us = {{0.125f, 0.1328125f, 0.609375f, 0.625f}, {0.125f, 0.14453125f, 0.61328125f, 0.625f}};
        float[][] vs = {{0.25f, 0.2890625f, 0.7265625f, 0.75f}, {0.75f, 0.734375f, 0.28125f, 0.25f}};
        Geometry.Slice9Params params = Params();
        for (int rotated = 0; rotated < 2; ++rotated) {
            for (byte flip = 0; flip < 4; ++flip) {
                params.flipHorizontal = (byte)(flip & 1);
                params.flipVertical = (byte)((flip >> 1) & 1);
                float[] uvs = GeometryJni.GenerateSlice9TexCoords(params, 256, 128, corners[rotated]);
                Check(uvs.length == 32);
                for (int i = 0; i < 16; ++i) {
                    int x = params.flipHorizontal == 0 ? i % 4 : 3 - i % 4;
                    int y = params.flipVertical == 0 ? i / 4 : 3 - i / 4;
                    Near(us[rotated][rotated == 0 ? x : y], uvs[i * 2]);
                    Near(vs[rotated][rotated == 0 ? y : x], uvs[i * 2 + 1]);
                }
            }
        }
    }

    // Verifies index conversion and that Java receives its own copy of native static data.
    private static void Indices() {
        int[] indices = GeometryJni.GetSlice9Indices();
        Check(indices.length == 54);
        for (int cell = 0; cell < 9; ++cell) {
            int bl = cell / 3 * 4 + cell % 3;
            int[] expected = {bl, bl + 1, bl + 5, bl, bl + 5, bl + 4};
            for (int i = 0; i < 6; ++i)
                Check(expected[i] == indices[cell * 6 + i]);
        }
        indices[0] = 123;
        Check(GeometryJni.GetSlice9Indices()[0] == 0);
    }

    private static void Invalid(Runnable action) {
        try {
            action.run();
            throw new AssertionError("Expected IllegalArgumentException");
        } catch (IllegalArgumentException expected) {
        }
    }

    // Verifies malformed Java inputs are rejected before native array access.
    private static void InvalidInputs() {
        Invalid(() -> GeometryJni.GenerateSlice9Positions(null));
        Invalid(() -> {
            Geometry.Slice9Params params = Params();
            params.slice9 = null;
            GeometryJni.GenerateSlice9Positions(params);
        });
        Invalid(() -> {
            Geometry.Slice9Params params = Params();
            params.slice9 = new float[3];
            GeometryJni.GenerateSlice9Positions(params);
        });
        Invalid(() -> {
            Geometry.Slice9Params params = Params();
            params.flipHorizontal = 2;
            GeometryJni.GenerateSlice9Positions(params);
        });
        Invalid(() -> GeometryJni.GenerateSlice9TexCoords(Params(), 256, 128, null));
        Invalid(() -> GeometryJni.GenerateSlice9TexCoords(Params(), 256, 128, new float[6]));
    }

    public static void main(String[] args) {
        // GeometryJni is deliberately the first API used, exercising library initialization.
        Positions();
        TexCoords();
        Indices();
        InvalidInputs();
        System.out.println("Geometry JNI tests passed");
    }
}
