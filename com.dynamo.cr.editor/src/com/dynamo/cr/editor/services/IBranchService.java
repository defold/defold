package com.dynamo.cr.editor.services;

import org.eclipse.ui.services.IDisposable;

import com.dynamo.cr.client.IBranchListener;

// TODO: Move this class to a new project called com.dynamo.cr.client.ui
public interface IBranchService extends IDisposable {

    public void updateBranchStatus();

    public void addBranchListener(IBranchListener branchListener);

    public void removeBranchListener(IBranchListener branchListener);
}
