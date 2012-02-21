package com.dynamo.cr.sceneed.core.test;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.cr.sceneed.test.proto.Dummy.DummyChildDesc;
import com.dynamo.cr.sceneed.test.proto.Dummy.DummyNodeDesc;

public class DummyNodeLoader implements INodeLoader<DummyNode, DummyNodeDesc> {

    @Override
    public DummyNode load(ILoaderContext context, InputStream contents) throws IOException, CoreException {
        DummyNodeDesc.Builder builder = DummyNodeDesc.newBuilder();
        LoaderUtil.loadBuilder(builder, contents);
        return load(context, builder.build());
    }

    @Override
    public DummyNode load(ILoaderContext context, DummyNodeDesc message) throws IOException, CoreException {
        DummyNode node = new DummyNode();
        for (DummyChildDesc childDesc : message.getChildrenList()) {
            node.addChild(context.loadNode(DummyChild.class, childDesc));
        }
        return node;
    }

    @Override
    public DummyNodeDesc buildMessage(ILoaderContext context, DummyNode node, IProgressMonitor monitor) throws IOException, CoreException {
        DummyNodeDesc.Builder builder = DummyNodeDesc.newBuilder();
        for (Node child : node.getChildren()) {
            builder.addChildren((DummyChildDesc)context.buildNodeMessage(child, monitor));
        }
        return builder.build();
    }

}
