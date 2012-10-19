package com.dynamo.cr.parted.nodes;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.Emitter.Builder;
import com.google.protobuf.Message;


public class EmitterLoader implements INodeLoader<EmitterNode> {

    public EmitterLoader() {
    }

    @Override
    public EmitterNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        Builder builder = Emitter.newBuilder();
        LoaderUtil.loadBuilder(builder, contents);
        return new EmitterNode(builder.build());
    }

    @Override
    public Message buildMessage(ILoaderContext context, EmitterNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        return null;
    }

}
