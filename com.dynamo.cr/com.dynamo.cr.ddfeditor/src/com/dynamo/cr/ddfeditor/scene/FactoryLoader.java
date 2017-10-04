package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.gamesystem.proto.GameSystem.FactoryDesc;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class FactoryLoader implements INodeLoader<FactoryNode> {

    @Override
    public FactoryNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        FactoryDesc.Builder builder = FactoryDesc.newBuilder();
        TextFormat.merge(reader, builder);
        FactoryDesc desc = builder.build();
        FactoryNode factory = new FactoryNode();
        factory.setPrototype(desc.getPrototype());
        factory.setLoadDynamically(desc.getLoadDynamically());
        return factory;
    }

    @Override
    public Message buildMessage(ILoaderContext context, FactoryNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        FactoryDesc.Builder builder = FactoryDesc.newBuilder();
        builder.setPrototype(node.getPrototype());
        builder.setLoadDynamically(node.getLoadDynamically());
        return builder.build();
    }

}
