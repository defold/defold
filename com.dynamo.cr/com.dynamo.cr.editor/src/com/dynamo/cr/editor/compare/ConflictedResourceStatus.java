package com.dynamo.cr.editor.compare;

import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status;

public class ConflictedResourceStatus extends ResourceStatus {

    public interface IResolveListener {
        public void handleResolve(ConflictedResourceStatus conflict);
    }

    public enum Resolve { UNRESOLVED, YOURS, THEIRS };

    private Resolve resolve;
    private java.util.List<IResolveListener> listeners;

    public ConflictedResourceStatus(Status status) {
        super(status);
        this.resolve = Resolve.UNRESOLVED;
        this.listeners = new java.util.ArrayList<IResolveListener>();
    }

    public Resolve getResolve() {
        return this.resolve;
    }

    public void setResolve(Resolve resolve) {
        if (this.resolve != resolve) {
            this.resolve = resolve;
            for (IResolveListener listener : this.listeners) {
                listener.handleResolve(this);
            }
        }
    }

    public void addResolveListener(IResolveListener listener) {
        this.listeners.add(listener);
    }
}
