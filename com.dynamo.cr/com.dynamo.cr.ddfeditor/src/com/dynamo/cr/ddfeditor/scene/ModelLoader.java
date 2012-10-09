package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.model.proto.Model.ModelDesc;
import com.dynamo.model.proto.Model.ModelDesc.Builder;
import com.google.protobuf.Message;

public class ModelLoader implements INodeLoader<ModelNode> {

    @Override
    public ModelNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {

        Builder b = ModelDesc.newBuilder();
        LoaderUtil.loadBuilder(b, contents);
        ModelDesc modelDesc = b.build();

        ModelNode node = new ModelNode(modelDesc);
        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, ModelNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        return node.buildMessage();
    }

}
