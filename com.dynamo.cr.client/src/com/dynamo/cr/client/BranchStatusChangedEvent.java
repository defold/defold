package com.dynamo.cr.client;

import java.util.EventObject;

import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;

@SuppressWarnings("serial")
public class BranchStatusChangedEvent extends EventObject {

    private Object[] resources;

    public BranchStatusChangedEvent(BranchStatus source, Object[] resources) {
        super(source);
        this.resources = resources;
    }

    public Object[] getResources() {
        return this.resources;
    }
}
