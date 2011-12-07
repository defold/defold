package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.google.protobuf.Message;

public class CameraLoader implements INodeLoader<CameraNode> {

    @Override
    public CameraNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        return new CameraNode();
    }

    @Override
    public Message buildMessage(ILoaderContext context, CameraNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        return null;
    }

}
