package com.dynamo.cr.client;

import java.util.EventListener;

public interface IBranchListener extends EventListener {

    public void branchStatusChanged(BranchStatusChangedEvent event);
}
