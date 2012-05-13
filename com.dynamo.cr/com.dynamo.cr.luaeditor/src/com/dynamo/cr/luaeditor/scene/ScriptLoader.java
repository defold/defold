package com.dynamo.cr.luaeditor.scene;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.google.protobuf.Message;

public class ScriptLoader implements INodeLoader<ScriptNode> {
    @Override
    public ScriptNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        return new ScriptNode();
    }

    @Override
    public Message buildMessage(ILoaderContext context, ScriptNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        // TODO Auto-generated method stub
        return null;
    }

}
