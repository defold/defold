package com.dynamo.cr.parted.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.nio.ByteBuffer;

import org.eclipse.swt.widgets.Display;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.parted.ParticleLibrary;
import com.dynamo.cr.parted.ParticleLibrary.Quat;
import com.dynamo.cr.parted.ParticleLibrary.RenderInstanceCallback;
import com.dynamo.cr.parted.ParticleLibrary.Vector3;
import com.dynamo.particle.proto.Particle.EmissionSpace;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.EmitterType;
import com.dynamo.particle.proto.Particle.ParticleFX;
import com.dynamo.particle.proto.Particle.PlayMode;
import com.dynamo.particle.proto.Particle.Texture_t;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.IntByReference;

public class ParticleSystemTest {

    private Pointer context;

    private static final int MAX_PARTICLE_COUNT = 1024;

    @Before
    public void setUp() throws Exception {
        // Avoid hang when running unit-test on Mac OSX
        // Related to SWT and threads?
        if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
            Display.getDefault();
        }

        context = ParticleLibrary.Particle_CreateContext(32, MAX_PARTICLE_COUNT);
    }

    @After
    public void tearDown() {
        ParticleLibrary.Particle_DestroyContext(context);
    }

    @Test
    public void smokeTest() throws IOException {
        Emitter.Builder eb = Emitter.newBuilder()
                .setMode(PlayMode.PLAY_MODE_ONCE)
                .setSpace(EmissionSpace.EMISSION_SPACE_WORLD)
                .setPosition(com.dynamo.proto.DdfMath.Point3.newBuilder().build())
                .setRotation(com.dynamo.proto.DdfMath.Quat.newBuilder().build())
                .setTexture(Texture_t.newBuilder().setName("foo").setTX(16).setTY(16))
                .setMaterial("test")
                .setMaxParticleCount(17)
                .setType(EmitterType.EMITTER_TYPE_SPHERE);
        ParticleFX.Builder pfxb = ParticleFX.newBuilder()
                .addEmitters(eb);

        byte[] pfxData = pfxb.build().toByteArray();

        Pointer prototype = ParticleLibrary.Particle_NewPrototype(ByteBuffer.wrap(pfxData), pfxData.length);
        Pointer instance = ParticleLibrary.Particle_CreateInstance(context, prototype);
        assertNotNull(instance);
        ParticleLibrary.Particle_SetPosition(context, instance, new Vector3(1, 2, 3));
        ParticleLibrary.Particle_SetRotation(context, instance, new Quat(0, 0, 0, 1));

        ParticleLibrary.Particle_StartInstance(context, instance);

        IntByReference out_size = new IntByReference(1234);
        // 6 vertices * 6 floats of 4 bytes
        int vertex_buffer_size = MAX_PARTICLE_COUNT * 6 * 6 * 4;
        ByteBuffer vertex_buffer = ByteBuffer.wrap(new byte[vertex_buffer_size]);
        ParticleLibrary.Particle_Update(context, 1.0f / 60.0f, vertex_buffer, vertex_buffer.capacity(), out_size);
        assertTrue(1234 != out_size.getValue());

        ParticleLibrary.Particle_Render(context, new Pointer(1122), new RenderInstanceCallback() {
            @Override
            public void invoke(Pointer userContext, Pointer material,
                    Pointer texture, int vertexIndex, int vertexCount) {

                assertEquals(new Pointer(1122), userContext);
                assertEquals(Pointer.NULL, material);
                assertEquals(Pointer.NULL, texture);
            }
        });

        ParticleLibrary.Particle_StopInstance(context, instance);
        ParticleLibrary.Particle_RestartInstance(context, instance);
        ParticleLibrary.Particle_DestroyInstance(context, instance);
        ParticleLibrary.Particle_DeletePrototype(prototype);
    }

}

