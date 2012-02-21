package com.dynamo.cr.sceneed.core.test;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.SceneUtil;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.cr.sceneed.test.proto.Dummy.DummyChildDesc;
import com.google.protobuf.Message;

public class DummyChildLoader implements INodeLoader<DummyChild, DummyChildDesc> {

    @Override
    public DummyChild load(ILoaderContext context, InputStream contents) throws IOException, CoreException {
        DummyChildDesc.Builder builder = DummyChildDesc.newBuilder();
        LoaderUtil.loadBuilder(builder, contents);
        return load(context, builder.build());
    }

    @Override
    public DummyChild load(ILoaderContext context, DummyChildDesc message) throws IOException, CoreException {
        ByteArrayInputStream stream = new ByteArrayInputStream(message.getData().getBytes());
        DummyChild node = (DummyChild)context.loadNode(message.getType(), stream);
        node.setIntVal(message.getIntVal());
        return node;
    }

    @Override
    public DummyChildDesc buildMessage(ILoaderContext context, DummyChild node, IProgressMonitor monitor) throws IOException, CoreException {
        DummyChildDesc.Builder builder = DummyChildDesc.newBuilder();
        builder.setIntVal(node.getIntVal());
        INodeType nodeType = context.getNodeTypeRegistry().getNodeTypeClass(node.getClass());
        builder.setType(nodeType.getExtension());
        Message message = context.buildNodeMessage(node, monitor);
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        SceneUtil.saveMessage(message, stream, monitor);
        builder.setData(stream.toString());
        return builder.build();
    }

}
