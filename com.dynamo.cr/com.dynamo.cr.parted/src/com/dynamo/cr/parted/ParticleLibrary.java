package com.dynamo.cr.parted;

import java.nio.Buffer;

import com.sun.jna.Callback;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;
import com.sun.jna.ptr.IntByReference;

public class ParticleLibrary {
    static {
        Native.register("particle_shared");
    }

    public interface RenderInstanceCallback extends Callback {
        void invoke(Pointer userContext, Pointer material, Pointer texture, int vertexIndex, int vertexCount);
    }

    public static native Pointer Particle_NewPrototype(Buffer emitterData, int emitterDataSize);

    public static native void Particle_DeletePrototype(Pointer prototype);

    public static native Pointer Particle_CreateContext(int maxEmitterCount, int maxParticleCount);

    public static native Pointer Particle_DestroyContext(Pointer context);

    public static native Pointer Particle_CreateInstance(Pointer context, Pointer prototype);

    public static native void Particle_DestroyInstance(Pointer context, Pointer instance);

    public static native void Particle_StartInstance(Pointer context, Pointer instance);

    public static native void Particle_StopInstance(Pointer context, Pointer instance);

    public static native void Particle_RestartInstance(Pointer context, Pointer instance);

    public static native void Particle_SetPosition(Pointer context, Pointer instance, Vector3 position);

    public static native void Particle_SetRotation(Pointer context, Pointer instance, Quat rotation);

    public static native void Particle_Update(Pointer context, float dt, Buffer vertexBuffer, int vertexBufferSize, IntByReference outVertexBufferSize);

    public static native void Particle_Render(Pointer context, Pointer userContext, RenderInstanceCallback callback);

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
    }

}
