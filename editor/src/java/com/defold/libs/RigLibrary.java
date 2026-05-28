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

import java.nio.Buffer;
import java.util.Arrays;
import java.util.List;

import javax.vecmath.Matrix4d;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.sun.jna.Native;
import com.sun.jna.NativeLibrary;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;
import com.sun.jna.ptr.IntByReference;

public class RigLibrary {
    private static Logger logger = LoggerFactory.getLogger(RigLibrary.class);

    static {
        try {
            ResourceUnpacker.unpackResources();
            Native.register(RigLibrary.class, NativeLibrary.getInstance(ResourceUnpacker.getPreloadedLibraryPath("rig_shared").toString()));
        } catch (Exception e) {
            logger.error("Failed to register bundled rig_shared", e);
        }
    }

    public static native Pointer Rig_CreatePreview(Buffer skeletonBuffer, int skeletonBufferSize, Buffer meshSetBuffer, int meshSetBufferSize, Buffer animationSetBuffer, int animationSetBufferSize);
    public static native void Rig_DestroyPreview(Pointer preview);
    public static native long Rig_Hash(String value);
    public static native int Rig_PlayAnimation(Pointer preview, long animationId);
    public static native int Rig_CancelAnimation(Pointer preview);
    public static native int Rig_Update(Pointer preview, float dt);
    public static native void Rig_ResetPoseMatrixCache(Pointer preview);
    public static native int Rig_AcquirePoseMatrixCacheEntry(Pointer preview);
    public static native int Rig_GetPoseMatrixCacheDataOffset(Pointer preview);
    public static native boolean Rig_HasPoseMatrixCacheAnimatedPose(Pointer preview);
    public static native int Rig_GetBoneCount(Pointer preview);
    public static native int Rig_WritePoseMatrixCache(Pointer preview, Buffer outRgba, int maxVec4Count);
    public static native int Rig_GetDeindexedVertexCount(Pointer preview, int modelIndex, int meshIndex);
    public static native int Rig_GenerateVertexData(Pointer preview, int modelIndex, int meshIndex, Matrix4 worldTransform, Matrix4 normalTransform, VertexAttributeInfos attributeInfos, Buffer vertexBuffer, int vertexBufferSize, IntByReference outVertexBufferSize);
    public static native boolean Rig_GetModelMatrix(Pointer preview, int modelIndex, boolean useBoneTransform, Buffer outMatrix16);

    public static class VertexAttributeInfo extends Structure {
        public long    nameHash;
        public int     semanticType;
        public int     dataType;
        public int     vectorType;
        public int     stepFunction;
        public int     coordinateSpace;
        public Pointer valuePtr;
        public int     valueVectorType;
        public int     elementCount;
        public boolean normalize;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("nameHash", "semanticType", "dataType", "vectorType", "stepFunction", "coordinateSpace", "valuePtr", "valueVectorType", "elementCount", "normalize");
        }
    }

    public static class VertexAttributeInfos extends Structure {
        public VertexAttributeInfos() {
            structSize = size();
        }

        public Pointer infos;
        public int     numInfos;
        public int     vertexStride;
        public int     structSize;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("infos", "numInfos", "vertexStride", "structSize");
        }
    }

    public static class Matrix4 extends Structure {
        public Matrix4() {
        }

        public Matrix4(Matrix4d m) {
            m00 = (float)m.m00;
            m10 = (float)m.m10;
            m20 = (float)m.m20;
            m30 = (float)m.m30;
            m01 = (float)m.m01;
            m11 = (float)m.m11;
            m21 = (float)m.m21;
            m31 = (float)m.m31;
            m02 = (float)m.m02;
            m12 = (float)m.m12;
            m22 = (float)m.m22;
            m32 = (float)m.m32;
            m03 = (float)m.m03;
            m13 = (float)m.m13;
            m23 = (float)m.m23;
            m33 = (float)m.m33;
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
}
