package com.dynamo.cr.target.core;

public class TargetChangedEvent {

    private ITargetService service;

    public TargetChangedEvent(ITargetService service) {
        this.service = service;
    }

    public ITargetService getService() {
        return service;
    }

}
