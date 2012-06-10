package com.dynamo.cr.targets.core;

public class TargetChangedEvent {

    private ITargetsService service;

    public TargetChangedEvent(ITargetsService service) {
        this.service = service;
    }

    public ITargetsService getService() {
        return service;
    }

}
