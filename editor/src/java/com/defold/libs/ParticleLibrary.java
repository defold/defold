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

package com.defold.libs;

import java.io.IOException;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.util.Arrays;
import java.util.List;

import javax.vecmath.Matrix4d;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.sun.jna.Callback;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;
import com.sun.jna.ptr.FloatByReference;
import com.sun.jna.ptr.IntByReference;

public class ParticleLibrary {
    private static Logger logger = LoggerFactory.getLogger(ParticleLibrary.class);

    static {
        try {
            ResourceUnpacker.unpackResources();
            Native.register("particle_shared");
        } catch (Exception e) {
            logger.error("Failed to extract/register particle_shared", e);
        }
    }

    public interface RenderInstanceCallback extends Callback {
        void invoke(Pointer userContext, Pointer material, Pointer texture, Matrix4 worldTransform, int blendMode, int vertexIndex, int vertexCount, Pointer constants, int constantCount);
    }

    public interface FetchResourcesCallback extends Callback {
        int invoke(FetchResourcesParams params, FetchResourcesData outData);
    }

    public static native Pointer Particle_NewPrototype(Buffer buffer, int bufferSize);

    public static native void Particle_DeletePrototype(Pointer prototype);

    public static native boolean Particle_ReloadPrototype(Pointer prototype, Buffer buffer, int bufferSize);

    public static native Pointer Particle_CreateContext(int maxEmitterCount, int maxParticleCount);

    public static native Pointer Particle_DestroyContext(Pointer context);

    public static native int Particle_GetContextMaxParticleCount(Pointer context);

    public static native void Particle_SetContextMaxParticleCount(Pointer context, int maxParticleCount);

    public static native Pointer Particle_CreateInstance(Pointer context, Pointer prototype, Pointer emitterStateCallbackData);

    public static native void Particle_DestroyInstance(Pointer context, Pointer instance);

    public static native void Particle_ReloadInstance(Pointer context, Pointer instance, boolean replay);

    public static native void Particle_StartInstance(Pointer context, Pointer instance);

    public static native void Particle_StopInstance(Pointer context, Pointer instance);

    public static native void Particle_ResetInstance(Pointer context, Pointer instance);

    public static native void Particle_SetPosition(Pointer context, Pointer instance, Vector3 position);

    public static native void Particle_SetRotation(Pointer context, Pointer instance, Quat rotation);

    public static native void Particle_SetScale(Pointer context, Pointer instance, float scale);

    public static native boolean Particle_IsSleeping(Pointer context, Pointer instance);

    public static native void Particle_Update(Pointer context, float dt, FetchResourcesCallback callback);

    public static native int Particle_GenerateVertexData(Pointer context, float dt, Pointer instance, int emitterIndex, VertexAttributeInfos attributeInfos, Vector4 color, Buffer vb, int vbMaxSize, IntByReference outVbSize);

    public static native void Particle_RenderEmitter(Pointer context, Pointer instance, int emitterIndex, Pointer userContext, RenderInstanceCallback callback);

    public static native void Particle_SetMaterial(Pointer prototype, int emitterIndex, Pointer material);

    public static native void Particle_SetTileSource(Pointer prototype, int emitterIndex, Pointer tileSource);

    public static native long Particle_Hash(String value);

    public static native void Particle_GetStats(Pointer context, Stats stats);

    public static native void Particle_GetInstanceStats(Pointer context, Pointer instance, InstanceStats stats);

    public static native int Particle_GetVertexBufferSize(int particle_count, int vertex_format);

    public static native void Particle_ResetAttributeScratchBuffer(Pointer context);

    public static native Pointer Particle_WriteAttributeToScratchBuffer(Pointer context, Buffer bytes, int byte_count);

    public static class VertexAttributeInfo extends Structure {
        public long    nameHash;
        public int     semanticType;
        public int     dataType;
        public int     vectorType;
        public int     stepFunction;
        public int     coordinateSpace;
        public Pointer valuePtr;
        public int     valueVectorType;
        public boolean normalize;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("nameHash", "semanticType", "dataType", "vectorType", "stepFunction", "coordinateSpace", "valuePtr", "valueVectorType", "normalize");
        }
    }

    public static class VertexAttributeInfos extends Structure {
        public VertexAttributeInfos() {
            structSize = size();
        }

        public VertexAttributeInfo[] infos = new VertexAttributeInfo[8]; // ==> dmGraphics::MAX_VERTEX_STREAM_COUNT
        public int                   vertexStride;
        public int                   numInfos;
        public int                   structSize;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("infos", "vertexStride", "numInfos", "structSize");
        }
    }

    public static class Stats extends Structure {

        public Stats() {
            structSize = size();
        }

        public int particles;
        public int maxParticles;
        public int structSize;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("particles", "maxParticles", "structSize");
        }

    }

    public static class InstanceStats extends Structure {

        public InstanceStats() {
            structSize = size();
        }

        public float time;
        public int structSize;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("time", "structSize");
        }

    }

    public static class Vector3 extends Structure {

        public Vector3() {
        }

        public Vector3(float x, float y, float z) {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public Vector3(Pointer position) {
            super(position);
        }

        public float x;
        public float y;
        public float z;
        public float pad;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("x", "y", "z", "pad");
        }
    }

    public static class Vector4 extends Structure {

        public Vector4() {
        }

        public Vector4(float x, float y, float z, float w) {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }

        public Vector4(Pointer position) {
            super(position);
        }

        public float x;
        public float y;
        public float z;
        public float w;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("x", "y", "z", "w");
        }
    }

    public static class Quat extends Structure {

        public Quat() {
        }

        public Quat(float x, float y, float z, float w) {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }

        public float x;
        public float y;
        public float z;
        public float w;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("x", "y", "z", "w");
        }
    }

    public static class Matrix4 extends Structure {
        public Matrix4() {
        }

        public Matrix4(Pointer position) {
            super(position);
        }

        public Matrix4d toMatrix() {
            Matrix4d m = new Matrix4d();
            m.m00 = m00;
            m.m10 = m10;
            m.m20 = m20;
            m.m30 = m30;
            m.m01 = m01;
            m.m11 = m11;
            m.m21 = m21;
            m.m31 = m31;
            m.m02 = m02;
            m.m12 = m12;
            m.m22 = m22;
            m.m32 = m32;
            m.m03 = m03;
            m.m13 = m13;
            m.m23 = m23;
            m.m33 = m33;
            return m;
        }

        public float m00;
        public float m10;
        public float m20;
        public float m30;
        public float m01;
        public float m11;
        public float m21;
        public float m31;
        public float m02;
        public float m12;
        public float m22;
        public float m32;
        public float m03;
        public float m13;
        public float m23;
        public float m33;
        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("m00", "m10", "m20", "m30", "m01", "m11", "m21", "m31", "m02", "m12", "m22", "m32", "m03", "m13", "m23", "m33");
        }
    }

    public static interface AnimPlayback {
        public static final int ANIM_PLAYBACK_NONE = 0;
        public static final int ANIM_PLAYBACK_ONCE_FORWARD = 1;
        public static final int ANIM_PLAYBACK_ONCE_BACKWARD = 2;
        public static final int ANIM_PLAYBACK_ONCE_PINGPONG = 6;
        public static final int ANIM_PLAYBACK_LOOP_FORWARD = 3;
        public static final int ANIM_PLAYBACK_LOOP_BACKWARD = 4;
        public static final int ANIM_PLAYBACK_LOOP_PINGPONG = 5;
    }

    public static interface FetchResourcesResult {
        public static final int FETCH_RESOURCES_OK = 0;
        public static final int FETCH_RESOURCES_NOT_FOUND = -1;
        public static final int FETCH_RESOURCES_UNKNOWN_ERROR = -1000;
    }

    public static class AnimationData extends Structure {
        public AnimationData() {
            super();
        }

        public Pointer texture;
        public FloatBuffer texCoords;
        public FloatBuffer texDims;
        public IntBuffer pageIndices;
        public IntBuffer frameIndices;
        public int playback;
        public int tileWidth;
        public int tileHeight;
        public int startTile;
        public int endTile;
        public int fps;
        public int hFlip;
        public int vFlip;
        // Used to validate the struct size in particle.cpp
        public int structSize;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("texture", "texCoords", "texDims", "pageIndices", "frameIndices", "playback", "tileWidth", "tileHeight", "startTile", "endTile", "fps", "hFlip", "vFlip", "structSize");
        }
    }

    public static class FetchResourcesParams extends Structure {
        public FetchResourcesParams() {
            super();
        }

        public Pointer particleContext;
        public int     instance;
        public long    animation;
        public int     emitterIndex;
        public Pointer materialResource;
        public Pointer textureSetResource;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("particleContext", "instance", "animation", "emitterIndex", "materialResource", "textureSetResource");
        }
    }

    public static class FetchResourcesData extends Structure {
        public FetchResourcesData() {
            super();
        }

        public AnimationData animationData;
        public Pointer       material;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("animationData", "material");
        }
    }
}
