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
import com.dynamo.cr.parted.ParticleLibrary.RenderEmitterCallback;
import com.dynamo.cr.parted.ParticleLibrary.Vector3;
import com.dynamo.particle.proto.Particle.EmissionSpace;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.EmitterType;
import com.dynamo.particle.proto.Particle.PlayMode;
import com.dynamo.particle.proto.Particle.Texture_t;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.IntByReference;

public class ParticleSystemTest {

    private Pointer context;


    @Before
    public void setUp() throws Exception {
        // Avoid hang when running unit-test on Mac OSX
        // Related to SWT and threads?
        if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
            Display.getDefault();
        }

        context = ParticleLibrary.Particle_CreateContext(32, 1024);
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
                .setRotation(com.dynamo.proto.DdfMath.Quat.newBuilder().build())
                .setTexture(Texture_t.newBuilder().setName("foo").setTX(16).setTY(16))
                .setMaterial("test")
                .setMaxParticleCount(17)
                .setType(EmitterType.EMITTER_TYPE_SPHERE);

        byte[] emitterData = eb.build().toByteArray();

        Pointer prototype = ParticleLibrary.Particle_NewPrototype(context, ByteBuffer.wrap(emitterData), emitterData.length);
        Pointer emitter = ParticleLibrary.Particle_CreateEmitter(context, prototype);
        assertNotNull(emitter);
        ParticleLibrary.Particle_SetPosition(context, emitter, new Vector3(1, 2, 3));
        ParticleLibrary.Particle_SetRotation(context, emitter, new Quat(0, 0, 0, 1));

        ParticleLibrary.Particle_StartEmitter(context, emitter);

        IntByReference out_size = new IntByReference(1234);
        ByteBuffer vertex_buffer = ByteBuffer.wrap(new byte[1024]);
        ParticleLibrary.Particle_Update(context, 1.0f / 60.0f, vertex_buffer, vertex_buffer.capacity(), out_size);
        assertTrue(1234 != out_size.getValue());

        ParticleLibrary.Particle_Render(context, new Pointer(1122), new RenderEmitterCallback() {
            @Override
            public void invoke(Pointer userContext, Vector3 position, Pointer material,
                    Pointer texture, int vertexIndex, int vertexCount) {

                assertEquals(1.0f, position.x, 0);
                assertEquals(2.0f, position.y, 0);
                assertEquals(3.0f, position.z, 0);
                assertEquals(new Pointer(1122), userContext);
                assertEquals(Pointer.NULL, material);
                assertEquals(Pointer.NULL, texture);
            }
        });

        ParticleLibrary.Particle_StopEmitter(context, emitter);
        ParticleLibrary.Particle_RestartEmitter(context, emitter);
        ParticleLibrary.Particle_DestroyEmitter(context, emitter);
        ParticleLibrary.Particle_DeletePrototype(context, prototype);
    }

}

