package com.dynamo.cr.parted.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import java.io.ByteArrayInputStream;
import java.io.IOException;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.parted.nodes.EmitterNode;
import com.dynamo.cr.parted.nodes.ParticleFXLoader;
import com.dynamo.cr.parted.nodes.ParticleFXNode;
import com.dynamo.cr.parted.operations.AddEmitterOperation;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.cr.sceneed.ui.LoaderContext;
import com.dynamo.particle.proto.Particle.EmissionSpace;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.EmitterKey;
import com.dynamo.particle.proto.Particle.EmitterType;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.ModifierType;
import com.dynamo.particle.proto.Particle.ParticleFX;
import com.dynamo.particle.proto.Particle.PlayMode;
import com.dynamo.particle.proto.Particle.SplinePoint;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.google.protobuf.TextFormat;

public class ParticleFXNodeTest extends AbstractNodeTest {

    private ParticleFXLoader loader;
    private ParticleFXNode particleFXNode;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();
        this.loader = new ParticleFXLoader();
        this.particleFXNode = registerAndLoadRoot(ParticleFXNode.class, "particlefx", this.loader);
    }

    @Test
    public void testLoad() throws Exception {
        // Initial dummy load test
    }

    @Test
    public void testAddEmitter() throws Exception {
        // Setup custom registry and loader-context
        // Corresponding from base-class are all mocked
        INodeTypeRegistry nr = Activator.getDefault().getNodeTypeRegistry();
        LoaderContext lc = new LoaderContext(null, nr);
        EmitterNode emitter = (EmitterNode) lc.loadNodeFromTemplate("emitter");

        execute(new AddEmitterOperation(particleFXNode, emitter, getPresenterContext()));
        assertThat(emitterCount(), is(1));

        ParticleFX particleFX = (ParticleFX) this.loader.buildMessage(getLoaderContext(), this.particleFXNode, null);
        assertThat(particleFX.getEmittersCount(), is(1));

        ParticleFXNode nodePrim = this.loader.load(getLoaderContext(), new ByteArrayInputStream(TextFormat.printToString(particleFX).getBytes()));
        assertThat(nodePrim.getChildren().size(), is(1));
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testScalarProperty() throws Exception {
        Emitter.Builder eb = emitterBuilder();

        EmitterKey key = EmitterKey.EMITTER_KEY_SPAWN_DELAY;
        eb.addProperties(Emitter.Property.newBuilder().setKey(key).addPoints(newPoint(0, 0.0f)));
        EmitterNode emitter = (EmitterNode) new EmitterNode(eb.build());

        IPropertyModel<EmitterNode, ISceneModel> m = (IPropertyModel<EmitterNode, ISceneModel>) emitter.getAdapter(IPropertyModel.class);
        ValueSpread vs = (ValueSpread) m.getPropertyValue(key.toString());
        ValueSpread tmp = new ValueSpread(vs);
        tmp.setValue(0.5f);
        IUndoableOperation op = m.setPropertyValue(key.toString(), tmp);
        execute(op);

        vs = (ValueSpread) m.getPropertyValue(key.toString());
        assertThat(vs.isAnimated(), is(false));
        assertThat(vs.getValue(), is(0.5));
        assertThat(vs.getSpread(), is(0.0));

        Emitter msg = emitter.buildMessage().build();
        assertThat(msg.getProperties(0).getPoints(0).getY(), is(0.5f));
    }

    @SuppressWarnings("unchecked")
    public void testSplineProperty() throws Exception {
        Emitter.Builder eb = emitterBuilder();

        EmitterKey key = EmitterKey.EMITTER_KEY_SPAWN_DELAY;
        eb.addProperties(Emitter.Property.newBuilder().setKey(key).addPoints(newPoint(0, 0.6f)).addPoints(newPoint(1, 1)) );
        EmitterNode emitter = (EmitterNode) new EmitterNode(eb.build());

        IPropertyModel<EmitterNode, ISceneModel> m = (IPropertyModel<EmitterNode, ISceneModel>) emitter.getAdapter(IPropertyModel.class);

        ValueSpread vs = (ValueSpread) m.getPropertyValue(key.toString());
        assertThat(vs.isAnimated(), is(true));
        assertThat(vs.getValue(), is(0.6));
        assertThat(vs.getSpread(), is(0.0));
    }

    @Test
    public void testModifier() throws Exception {
        Emitter.Builder eb = emitterBuilder().addModifiers(modifierBuilder().setType(ModifierType.MODIFIER_TYPE_ACCELERATION));
        EmitterNode node = new EmitterNode(eb.build());
        assertThat(node.buildMessage().getModifiersCount(), is(1));
    }

    private Emitter.Builder emitterBuilder() {
        Emitter.Builder eb = Emitter.newBuilder()
                .setMode(PlayMode.PLAY_MODE_LOOP)
                .setSpace(EmissionSpace.EMISSION_SPACE_EMITTER)
                .setPosition(Point3.newBuilder())
                .setRotation(Quat.newBuilder())
                .setTileSource("")
                .setAnimation("")
                .setMaterial("")
                .setMaxParticleCount(100)
                .setType(EmitterType.EMITTER_TYPE_SPHERE);
        return eb;
    }

    private Modifier.Builder modifierBuilder() {
        Modifier.Builder eb = Modifier.newBuilder()
                .setUseDirection(0)
                .setPosition(Point3.newBuilder())
                .setRotation(Quat.newBuilder());
        return eb;
    }

    private SplinePoint newPoint(float x, float y) {
        return SplinePoint.newBuilder().setX(x).setY(y).setTX(1).setTY(0).build();
    }

    private int emitterCount() {
        return particleFXNode.getChildren().size();
    }

}
