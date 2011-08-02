package com.dynamo.cr.editor.services;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IMarker;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceVisitor;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.jobs.Job;

import com.dynamo.cr.client.BranchStatusChangedEvent;
import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.IBranchListener;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.compare.ResourceStatus;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.markers.BranchStatusMarker;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;

public class BranchService implements IBranchService {

    private boolean jobScheduled = false;
    private long previouslyScheduled = System.currentTimeMillis();
    private java.util.List<ResourceStatus> resourceStatuses;
    private java.util.List<IBranchListener> branchListeners;

    public BranchService() {
        this.resourceStatuses = new java.util.ArrayList<ResourceStatus>();
        this.branchListeners = new ArrayList<IBranchListener>();
    }

    @Override
    public void dispose() {
        this.branchListeners.clear();
        this.resourceStatuses.clear();
    }

    @Override
    public void updateBranchStatus(Set<IResource> candidates) {
        update(candidates);
    }

    private IResource locateResource(String name) {
        IFolder contentRoot = EditorUtil.getContentRoot(Activator.getDefault().getProject());
        return contentRoot.findMember(name);
    }

    private void clearAllMarkers() throws CoreException {
        ResourcesPlugin.getWorkspace().getRoot().accept(new IResourceVisitor() {
            @Override
            public boolean visit(IResource resource) throws CoreException {
                resource.deleteMarkers(BranchStatusMarker.MARKER_ID, true, 0);
                return true;
            }
        });
    }

    private void doUpdate(Set<IResource> candidates) throws RepositoryException, CoreException {
        List<IResource> resources = new ArrayList<IResource>(32);

        if (candidates != null) {
            // Remove markers for all candidates
            for (IResource resource : candidates) {
                if (resource.isAccessible()) {
                    resource.deleteMarkers(BranchStatusMarker.MARKER_ID, true, 0);
                    resources.add(resource);
                }
            }
        } else {
            // We have no changed candidates, clear all markers and hence remove all decorators
            clearAllMarkers();
        }


        IBranchClient client = Activator.getDefault().getBranchClient();
        // We could be disconnected
        if (client == null) {
            return;
        }

        BranchStatus branchStatus = client.getBranchStatus();

        for (BranchStatus.Status status : branchStatus.getFileStatusList()) {
            IResource resource = locateResource(status.getName());
            if (resource != null) {
                resources.add(resource);
                resource.deleteMarkers(BranchStatusMarker.MARKER_ID, true, 0);
                IMarker branchStatusMarker = resource.createMarker(BranchStatusMarker.MARKER_ID);
                String[] attributeNames = new String[] {IMarker.SEVERITY, BranchStatusMarker.INDEX_STATUS_ATTRIBUTE_ID, BranchStatusMarker.WORKING_TREE_STATUS_ATTRIBUTE_ID};
                Object[] values = new Object[] {IMarker.SEVERITY_INFO, status.getIndexStatus(), status.getWorkingTreeStatus()};
                branchStatusMarker.setAttributes(attributeNames, values);
            }
        }
        fireBranchStatusChangedEvent(new BranchStatusChangedEvent(branchStatus, resources.toArray(new IResource[resources.size()])));
    }

    public void update(final Set<IResource> changedResources) {
        if (jobScheduled)
            return;

        jobScheduled = true;

        Job job = new Job("foo") {
            @Override
            protected IStatus run(IProgressMonitor monitor) {
                // We must set this to false early. Otherwise we could "miss" the last job
                jobScheduled = false;

                try {
                    doUpdate(changedResources);
                } catch (RepositoryException e) {
                    Activator.logException(e);
                } catch (CoreException e) {
                    Activator.logException(e);
                }
                return Status.OK_STATUS;
            }
        };
        job.setSystem(true);

        // Do not schedule jobs too frequently but schedule the first, in a burst series, immediately
        if (System.currentTimeMillis() - previouslyScheduled > 1000) {
            job.schedule();
        } else {
            job.schedule(1000);
        }
        previouslyScheduled = System.currentTimeMillis();
    }

    @Override
    public synchronized void addBranchListener(IBranchListener branchListener) {
        this.branchListeners.add(branchListener);
    }

    @Override
    public synchronized void removeBranchListener(IBranchListener branchListener) {
        this.branchListeners.remove(branchListener);
    }

    private synchronized void fireBranchStatusChangedEvent(BranchStatusChangedEvent event) {
        for (IBranchListener branchListener : this.branchListeners) {
            branchListener.branchStatusChanged(event);
        }
    }

}
