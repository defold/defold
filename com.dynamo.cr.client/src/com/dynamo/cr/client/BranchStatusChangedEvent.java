package com.dynamo.cr.client;

import java.util.EventObject;

import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;

@SuppressWarnings("serial")
public class BranchStatusChangedEvent extends EventObject {

    private Object[] resources;
    private BranchStatus branchStatus;

    public BranchStatusChangedEvent(BranchStatus source, Object[] resources) {
        super(source);
        this.branchStatus = source;
        this.resources = resources;
    }

    public Object[] getResources() {
        return this.resources;
    }

    public BranchStatus getBranchStatus() {
        return branchStatus;
    }

}
