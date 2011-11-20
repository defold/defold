package com.dynamo.cr.sceneed.ui.test;

import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.sceneed.core.Node;

public class TestNode extends Node {

    public void addChild(TestNode child) {
        super.addChild(child);
    }

    @Override
    protected Class<? extends NLS> getMessages() {
        return null;
    }

}
