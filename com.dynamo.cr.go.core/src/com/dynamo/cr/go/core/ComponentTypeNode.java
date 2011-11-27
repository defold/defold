package com.dynamo.cr.go.core;

import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.sceneed.core.Node;

public abstract class ComponentTypeNode extends Node {

    @Override
    protected Class<? extends NLS> getMessages() {
        return Messages.class;
    }

}
