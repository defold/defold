package com.dynamo.cr.editor.services;

import java.util.Set;

import org.eclipse.core.resources.IResource;
import org.eclipse.ui.services.IDisposable;

import com.dynamo.cr.client.IBranchListener;

// TODO: Move this class to a new project called com.dynamo.cr.client.ui
public interface IBranchService extends IDisposable {
    /**
     * Update branch status. The set of resources contains all potential changes.
     * The list could be a superset of the actual branch status, ie for reverted resources.
     * This is required to accurately remove markers for resources not in the branch status set
     * @param changedResource set of changed resource. Set to null when the set of changed resources is unknown. For null-argument all markers could potentially be
     * cleared and is hence less efficient
     */
    public void updateBranchStatus(Set<IResource> changedResource);

    public void addBranchListener(IBranchListener branchListener);

    public void removeBranchListener(IBranchListener branchListener);
}
