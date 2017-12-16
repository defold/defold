package com.defold.libs;

import java.io.IOException;
import java.nio.Buffer;
import java.nio.FloatBuffer;
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
import com.sun.jna.ptr.PointerByReference;

public class RigLibrary {
    private static Logger logger = LoggerFactory.getLogger(RigLibrary.class);

    static {
        try {
            ResourceUnpacker.unpackResources();
            Native.register("rig_shared");
        } catch (Exception e) {
            logger.error("Failed to extract/register rig_shared", e);
        }
    }

    public static interface Result {
        public static final int RESULT_OK = 0;
        public static final int RESULT_ERROR = 1;
        public static final int RESULT_ANIM_NOT_FOUND = 2;
    }

    public static interface RigMeshType {
        public static final int RIG_SPINE = 1;
        public static final int RIG_MODEL = 2;
    }

    public static interface RigPlayback {
        public static final int PLAYBACK_NONE          = 0;
        public static final int PLAYBACK_ONCE_FORWARD  = 1;
        public static final int PLAYBACK_ONCE_BACKWARD = 2;
        public static final int PLAYBACK_ONCE_PINGPONG = 3;
        public static final int PLAYBACK_LOOP_FORWARD  = 4;
        public static final int PLAYBACK_LOOP_BACKWARD = 5;
        public static final int PLAYBACK_LOOP_PINGPONG = 6;
        public static final int PLAYBACK_COUNT = 7;
    }

    public static interface RigVertexFormat {
        public static final int RIG_VERTEX_FORMAT_SPINE = 0;
        public static final int RIG_VERTEX_FORMAT_MODEL = 1;
    }

    public static class NewContextParams extends Structure {
        public PointerByReference context;
        public int maxRigInstanceCount;

        public NewContextParams(int maxRigInstanceCount) {
            this.maxRigInstanceCount = maxRigInstanceCount;
        }

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("context", "maxRigInstanceCount");
        }
    }

    public static native int Rig_NewContext(NewContextParams params);
    public static native void Rig_DeleteContext(Pointer context);
    public static native int Rig_Update(Pointer context, float dt);

    public static native Buffer Rig_GenerateVertexData(Pointer context, Pointer instance, Matrix4 modelMatrix, Matrix4 normal_matrix, Vector4 color, int vertexFormat, Buffer vertexDataOut);
    public static native int Rig_GetVertexCount(Pointer instance);

    public static native Pointer Rig_InstanceCreate(Pointer context, Buffer skeleton, int skeletonSize, Buffer meshSet, int meshSetSize, Buffer animationSet, int animationSetSize, long meshId, long defaultAnimation);
    public static native boolean Rig_InstanceDestroy(Pointer context, Pointer instance);

    public static native int Rig_PlayAnimation(Pointer instance, long animationId, int playback, float blendDuration, float offset, float playbackRate);
    public static native int Rig_CancelAnimation(Pointer instance);
    public static native int Rig_GetAnimation(Pointer instance);

    public static class Vector4 extends Structure {

        public Vector4() {
        }

        public Vector4(float v) {
            this.x = v;
            this.y = v;
            this.z = v;
            this.w = v;
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

        public static Matrix4 fromMatrix4d(Matrix4d src) {
            Matrix4 m = new Matrix4();
            m.m00 = (float)src.m00;
            m.m10 = (float)src.m10;
            m.m20 = (float)src.m20;
            m.m30 = (float)src.m30;
            m.m01 = (float)src.m01;
            m.m11 = (float)src.m11;
            m.m21 = (float)src.m21;
            m.m31 = (float)src.m31;
            m.m02 = (float)src.m02;
            m.m12 = (float)src.m12;
            m.m22 = (float)src.m22;
            m.m32 = (float)src.m32;
            m.m03 = (float)src.m03;
            m.m13 = (float)src.m13;
            m.m23 = (float)src.m23;
            m.m33 = (float)src.m33;
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
}
