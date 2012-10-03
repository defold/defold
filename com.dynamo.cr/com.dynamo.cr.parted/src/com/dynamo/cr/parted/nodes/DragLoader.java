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


public class DragLoader implements INodeLoader<DragNode> {

    public DragLoader() {
    }

    @Override
    public DragNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        Modifier.Builder builder = Modifier.newBuilder();
        LoaderUtil.loadBuilder(builder, contents);
        return new DragNode(builder.build());
    }

    @Override
    public Message buildMessage(ILoaderContext context, DragNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        return null;
    }

}
