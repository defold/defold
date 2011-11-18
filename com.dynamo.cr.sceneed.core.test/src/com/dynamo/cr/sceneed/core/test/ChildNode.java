package com.dynamo.cr.sceneed.core.test;

import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.sceneed.core.Messages;
import com.dynamo.cr.sceneed.core.Node;

public class ChildNode extends Node {

    public ChildNode() {
        // TODO Auto-generated constructor stub
    }

    @Override
    protected Class<? extends NLS> getMessages() {
        return Messages.class;
    }

}
