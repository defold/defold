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

    public interface RenderEmitterCallback extends Callback {
        void invoke(Pointer userContext, Vector3 position, Pointer material, Pointer texture, int vertexIndex, int vertexCount);
    }

    public static native Pointer Particle_NewPrototype(Pointer context, Buffer emitter_data, int emitterDataSize);
    public static native void Particle_DeletePrototype(Pointer context, Pointer prototype);
    public static native Pointer Particle_CreateContext(int maxEmitterCount, int maxParticleCount);
    public static native Pointer Particle_DestroyContext(Pointer context);
    public static native Pointer Particle_CreateEmitter(Pointer context, Pointer prototype);
    public static native void Particle_DestroyEmitter(Pointer context, Pointer emitter);
    public static native void Particle_StartEmitter(Pointer context, Pointer emitter);
    public static native void Particle_StopEmitter(Pointer context, Pointer emitter);
    public static native void Particle_RestartEmitter(Pointer context, Pointer emitter);
    public static native void Particle_SetPosition(Pointer context, Pointer emitter, Vector3 position);
    public static native void Particle_SetRotation(Pointer context, Pointer emitter, Quat rotation);
    public static native void Particle_Update(Pointer context, float dt, Buffer vertexBuffer, int vertexBufferSize, IntByReference outVertexBufferSize);
    public static native void Particle_Render(Pointer context, Pointer userContext, RenderEmitterCallback callback);

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
