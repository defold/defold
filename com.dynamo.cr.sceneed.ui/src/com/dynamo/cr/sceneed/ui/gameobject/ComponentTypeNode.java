package com.dynamo.cr.sceneed.ui.gameobject;

import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.sceneed.ui.Messages;
import com.dynamo.cr.sceneed.core.Node;

public abstract class ComponentTypeNode extends Node {

    public abstract String getTypeId();
    public abstract String getTypeName();

    @Override
    public final String toString() {
        return getTypeName();
    }

    @Override
    protected Class<? extends NLS> getMessages() {
        return Messages.class;
    }

}
