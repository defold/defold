package com.dynamo.cr.sceneed.core.test;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.cr.sceneed.test.proto.Dummy.EmbeddedChildDesc;

public class EmbeddedChildLoader implements INodeLoader<EmbeddedChild, EmbeddedChildDesc> {

    @Override
    public EmbeddedChild load(ILoaderContext context, InputStream contents) throws IOException, CoreException {
        EmbeddedChildDesc.Builder builder = EmbeddedChildDesc.newBuilder();
        LoaderUtil.loadBuilder(builder, contents);
        return load(context, builder.build());
    }

    @Override
    public EmbeddedChild load(ILoaderContext context, EmbeddedChildDesc message) throws IOException, CoreException {
        EmbeddedChild node = new EmbeddedChild();
        node.setEmbedVal(message.getEmbedVal());
        return node;
    }

    @Override
    public EmbeddedChildDesc buildMessage(ILoaderContext context, EmbeddedChild node, IProgressMonitor monitor) throws IOException, CoreException {
        EmbeddedChildDesc.Builder builder = EmbeddedChildDesc.newBuilder();
        builder.setEmbedVal(node.getEmbedVal());
        return builder.build();
    }

}
