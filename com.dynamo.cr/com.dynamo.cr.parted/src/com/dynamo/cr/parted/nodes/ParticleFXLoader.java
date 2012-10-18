package com.dynamo.cr.parted.nodes;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.ParticleFX;
import com.dynamo.particle.proto.Particle.ParticleFX.Builder;
import com.google.protobuf.Message;


public class ParticleFXLoader implements INodeLoader<ParticleFXNode> {

    public ParticleFXLoader() {
    }

    @Override
    public ParticleFXNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {

        Builder builder = ParticleFX.newBuilder();
        LoaderUtil.loadBuilder(builder, contents);

        ParticleFXNode node = new ParticleFXNode();
        ParticleFX particleFX = builder.build();
        for (Emitter e : particleFX.getEmittersList() ) {
            EmitterNode en = new EmitterNode(e);
            node.addChild(en);
        }
        for (Modifier m : particleFX.getModifiersList()) {
            node.addChild(ParticleUtils.createModifierNode(m));
        }

        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, ParticleFXNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        Builder b = ParticleFX.newBuilder();

        for (Node n : node.getChildren()) {
            if (n instanceof EmitterNode) {
                EmitterNode e = (EmitterNode) n;
                Emitter.Builder eb = e.buildMessage();
                b.addEmitters(eb);
            } else {
                AbstractModifierNode m = (AbstractModifierNode) n;
                b.addModifiers(m.buildMessage());
            }
        }

        return b.build();
    }

}
