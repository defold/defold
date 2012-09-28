package com.dynamo.cr.parted;

import java.nio.ByteBuffer;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.NullProgressMonitor;

import com.dynamo.cr.parted.ParticleLibrary.Quat;
import com.dynamo.cr.parted.ParticleLibrary.Vector3;
import com.dynamo.cr.parted.curve.HermiteSpline;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.Node;
import com.google.protobuf.Message;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.IntByReference;

public class ParticleFXNode extends Node {

    private static final long serialVersionUID = 1L;

    @Property
    private ValueSpread test = new ValueSpread();

    private Pointer prototype;
    private Pointer instance;
    private Pointer context;
    private boolean reset;

    public ParticleFXNode() {
        test.setCurve(new HermiteSpline());
    }

    public ParticleFXNode(Vector4d translation, Quat4d rotation) {
        super(translation, rotation);
    }

    @Override
    public void dispose() {
       super.dispose();
       if (instance != null) {
           ParticleLibrary.Particle_DestroyInstance(context, instance);
       }
       if (prototype != null) {
           ParticleLibrary.Particle_DeletePrototype(prototype);
       }
    }

    private void createInstance(Pointer context) {
        if (instance != null && !reset) {
            // TODO: Recreation predicate
            return;
        }
        reset = false;

        if (instance != null) {
            ParticleLibrary.Particle_DeletePrototype(prototype);
            ParticleLibrary.Particle_DestroyInstance(context, instance);
        }

        if (this.context == null) {
            this.context = context;
        } else {
            if (this.context != context) {
                throw new IllegalArgumentException("createInstance called with different context");
            }
        }

        ParticleFXLoader loader = new ParticleFXLoader();
        Message msg;
        try {
            msg = loader.buildMessage(null, this, new NullProgressMonitor());
            byte[] pfxData = msg.toByteArray();
            prototype = ParticleLibrary.Particle_NewPrototype(ByteBuffer.wrap(pfxData), pfxData.length);
            instance = ParticleLibrary.Particle_CreateInstance(context, prototype);
            ParticleLibrary.Particle_SetPosition(context, instance, new Vector3(0, 0, 0));
            ParticleLibrary.Particle_SetRotation(context, instance, new Quat(0, 0, 0, 1));
            ParticleLibrary.Particle_StartInstance(context, instance);
        } catch (Exception e) {
            // Logging here might be dangerous (inside simulation loop)
            e.printStackTrace();
        }
    }

    public void simulate(Pointer context, ByteBuffer vertexBuffer, double dt) {
        createInstance(context);
        IntByReference outSize = new IntByReference(0);

        if (dt > 0) {
            ParticleLibrary.Particle_Update(context, (float) dt, vertexBuffer, vertexBuffer.capacity(), outSize, null);
        } else {
            ParticleLibrary.Particle_RestartInstance(context, instance);
        }
    }

    public ValueSpread getTest() {
        return new ValueSpread(test);
    }

    public void setTest(ValueSpread test) {
        this.test.set(test);
    }

    public void reset() {
        this.reset = true;
    }

}
