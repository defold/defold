package com.dynamo.cr.parted.nodes;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.particle.proto.Particle.Modifier;
import com.google.protobuf.Message;


public class RadialLoader implements INodeLoader<RadialNode> {

    public RadialLoader() {
    }

    @Override
    public RadialNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        Modifier.Builder builder = Modifier.newBuilder();
        LoaderUtil.loadBuilder(builder, contents);
        return new RadialNode(builder.build());
    }

    @Override
    public Message buildMessage(ILoaderContext context, RadialNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        return null;
    }

}
