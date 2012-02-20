package com.dynamo.cr.sceneed.core.test;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.test.proto.Dummy.DummyChildDesc;
import com.dynamo.cr.sceneed.test.proto.Dummy.DummyNodeDesc;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class DummyLoader implements INodeLoader {

    @Override
    public Node load(ILoaderContext context, Class<? extends Node> nodeClass, InputStream contents) throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        if (nodeClass == DummyNode.class) {
            DummyNodeDesc.Builder builder = DummyNodeDesc.newBuilder();
            TextFormat.merge(reader, builder);
            DummyNodeDesc desc = builder.build();
            return load(context, nodeClass, desc);
        } else if (nodeClass == DummyChild.class) {
            DummyChildDesc.Builder builder = DummyChildDesc.newBuilder();
            TextFormat.merge(reader, builder);
            DummyChildDesc desc = builder.build();
            return load(context, nodeClass, desc);
        }
        return null;
    }

    private Node load(ILoaderContext context, Class<? extends Node> nodeClass, Message message) throws IOException, CoreException {
        if (nodeClass == DummyNode.class) {
            DummyNodeDesc nodeDesc = (DummyNodeDesc)message;
            DummyNode node = new DummyNode();
            for (DummyChildDesc childDesc : nodeDesc.getChildrenList()) {
                Node child = load(context, DummyChild.class, childDesc);
                node.addChild(child);
            }
            return node;
        } else if (nodeClass == DummyChild.class) {
            DummyChildDesc childDesc = (DummyChildDesc)message;
            DummyChild child = new DummyChild();
            child.setIntVal(childDesc.getIntVal());
            return child;
        }
        return null;
    }


    @Override
    public Message buildMessage(ILoaderContext context, Node node, IProgressMonitor monitor) throws IOException, CoreException {
        if (node instanceof DummyNode) {
            DummyNode dummy = (DummyNode)node;
            DummyNodeDesc.Builder builder = DummyNodeDesc.newBuilder();
            for (Node child : dummy.getChildren()) {
                builder.addChildren((DummyChildDesc)buildMessage(context, child, monitor));
            }
            return builder.build();
        } else if (node instanceof DummyChild) {
            DummyChild child = (DummyChild)node;
            DummyChildDesc.Builder builder = DummyChildDesc.newBuilder();
            builder.setIntVal(child.getIntVal());
            return builder.build();
        }
        return null;
    }

}
