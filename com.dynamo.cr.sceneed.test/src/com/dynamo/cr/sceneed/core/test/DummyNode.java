package com.dynamo.cr.sceneed.core.test;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.google.protobuf.Message;

public class DummyNode extends Node {

    public static class DummyLoader implements INodeLoader<DummyNode> {
        @Override
        public DummyNode load(ILoaderContext context, InputStream contents) throws IOException, CoreException {
            return new DummyNode();
        }

        @Override
        public Message buildMessage(ILoaderContext context, DummyNode node, IProgressMonitor monitor) throws IOException, CoreException {
            return null;
        }
    }

    @Property
    private int dummyProperty;

    public int getDummyProperty() {
        return this.dummyProperty;
    }

    public void setDummyProperty(int dummyProperty) {
        this.dummyProperty = dummyProperty;
    }

    public void addChild(DummyNode child) {
        super.addChild(child);
    }

    @Override
    public boolean handleReload(IFile file) {
        return file.exists();
    }
}
