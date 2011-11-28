package com.dynamo.cr.sceneed.core.test;

import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.sceneed.core.Node;

public class DummyNode extends Node {

    public void addChild(DummyNode child) {
        super.addChild(child);
    }

    @Override
    protected Class<? extends NLS> getMessages() {
        return null;
    }

}
