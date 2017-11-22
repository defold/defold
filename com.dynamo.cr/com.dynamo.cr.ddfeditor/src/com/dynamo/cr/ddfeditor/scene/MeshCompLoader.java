package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.mesh.proto.MeshProto.MeshDesc;
import com.dynamo.mesh.proto.MeshProto.MeshDesc.Builder;
import com.google.protobuf.Message;

public class MeshCompLoader implements INodeLoader<MeshCompNode> {

    @Override
    public MeshCompNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {

        Builder b = MeshDesc.newBuilder();
        LoaderUtil.loadBuilder(b, contents);
        MeshDesc meshDesc = b.build();

        MeshCompNode node = new MeshCompNode(meshDesc);
        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, MeshCompNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        return node.buildMessage();
    }

}
