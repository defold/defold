package com.dynamo.cr.parted.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import java.io.ByteArrayInputStream;
import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.parted.AddEmitterOperation;
import com.dynamo.cr.parted.EmitterNode;
import com.dynamo.cr.parted.ParticleFXLoader;
import com.dynamo.cr.parted.ParticleFXNode;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.cr.sceneed.ui.LoaderContext;
import com.dynamo.particle.proto.Particle.ParticleFX;
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

    private int emitterCount() {
        return particleFXNode.getChildren().size();
    }

}
