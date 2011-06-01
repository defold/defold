package com.dynamo.cr.editor.services;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.Semaphore;

import org.eclipse.core.resources.IMarker;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.client.BranchStatusChangedEvent;
import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.IBranchListener;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.compare.ResourceStatus;
import com.dynamo.cr.markers.BranchStatusMarker;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;

public class BranchService implements IBranchService {

    private BranchStatusUpdater updater;

    public BranchService() {
        this.updater = new BranchStatusUpdater();
        this.updater.addBranchListener(Activator.getDefault());
    }

    @Override
    public void dispose() {
        this.updater.dispose();
    }

    @Override
    public void updateBranchStatus() {
        if (this.updater.isAlive()) {
            this.updater.update();
        } else {
            this.updater.start();
        }
    }

    @Override
    public void addBranchListener(IBranchListener branchListener) {
        this.updater.addBranchListener(branchListener);
    }

    @Override
    public void removeBranchListener(IBranchListener branchListener) {
        this.updater.removeBranchListener(branchListener);
    }

    private class BranchStatusUpdater extends Thread {

        private boolean quit;
        Semaphore semaphore;
        java.util.List<ResourceStatus> resourceStatuses;
        java.util.List<IBranchListener> branchListeners;
        Semaphore listenerSemaphore;

        public BranchStatusUpdater() {
            super();
            this.quit = false;
            this.semaphore = new Semaphore(1);
            this.resourceStatuses = new java.util.ArrayList<ResourceStatus>();
            this.branchListeners = new ArrayList<IBranchListener>();
            this.listenerSemaphore = new Semaphore(1);
        }

        @Override
        public void run() {

            while (!quit) {
                try {
                    this.semaphore.acquire();
                    this.semaphore.drainPermits();

                    IBranchClient client = Activator.getDefault().getBranchClient();
                    if (client != null && Activator.getDefault().getProject() != null) {
                        try {
                            BranchStatus branchStatus = client.getBranchStatus();

                            final java.util.List<ResourceStatus> newResourceStatuses = new java.util.ArrayList<ResourceStatus>(branchStatus.getFileStatusCount());
                            for (com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status s : branchStatus.getFileStatusList()) {
                                ResourceStatus resourceStatus = new ResourceStatus(s);
                                if (resourceStatus.getResource() != null) {
                                    newResourceStatuses.add(resourceStatus);
                                }
                            }

                            boolean changed = false;
                            if (newResourceStatuses.size() == resourceStatuses.size()) {
                                for (int i = 0; i < resourceStatuses.size(); i++) {
                                    com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status currentStatus = resourceStatuses.get(i).getStatus();
                                    com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status status = newResourceStatuses.get(i).getStatus();
                                    if (!currentStatus.getName().equals(status.getName())
                                            || !currentStatus.getIndexStatus().equals(status.getIndexStatus())
                                            || !currentStatus.getWorkingTreeStatus().equals(status.getWorkingTreeStatus())) {
                                        changed = true;
                                        break;
                                    }
                                }
                            } else {
                                changed = true;
                            }

                            try {
                                if (changed) {
                                    final Set<IResource> resources = new HashSet<IResource>();
                                    for (ResourceStatus s : resourceStatuses) {
                                        IResource resource = s.getResource();
                                        if (resource != null) {
                                            resource.deleteMarkers(BranchStatusMarker.MARKER_ID, true, 0);
                                            resources.add(resource);
                                        }
                                    }
                                    for (ResourceStatus s : newResourceStatuses) {
                                        IResource resource = s.getResource();
                                        if (resource != null) {
                                            IMarker branchStatusMarker = resource.createMarker(BranchStatusMarker.MARKER_ID);
                                            String[] attributeNames = new String[] {IMarker.SEVERITY, BranchStatusMarker.INDEX_STATUS_ATTRIBUTE_ID, BranchStatusMarker.WORKING_TREE_STATUS_ATTRIBUTE_ID};
                                            Object[] values = new Object[] {IMarker.SEVERITY_INFO, s.getStatus().getIndexStatus(), s.getStatus().getWorkingTreeStatus()};
                                            branchStatusMarker.setAttributes(attributeNames, values);
                                            resources.add(resource);
                                        }
                                    }
                                    resourceStatuses = newResourceStatuses;
                                    fireBranchStatusChangedEvent(new BranchStatusChangedEvent(branchStatus, resources.toArray()));
                                }
                            } catch (CoreException e) {
                                e.printStackTrace();
                            }
                        } catch (RepositoryException e) {
                            Activator.getDefault().logger.warning(e.getMessage());
                        }
                    } else {
                        if (!resourceStatuses.isEmpty()) {
                            resourceStatuses.clear();
                        }
                    }

                    Thread.sleep(1000); // Do not update too often
                } catch (InterruptedException e) {
                    // This is fine
                }
            }
        }

        public void dispose() {
            this.branchListeners.clear();
            this.resourceStatuses.clear();
            this.quit  = true;
            this.interrupt();
            try {
                this.join();
            } catch (InterruptedException e) {
            }
        }

        public void update() {
            this.semaphore.release();
        }

        public void addBranchListener(IBranchListener branchListener) {
            try {
                this.listenerSemaphore.acquire();
                this.listenerSemaphore.drainPermits();
                this.branchListeners.add(branchListener);
                this.listenerSemaphore.release();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        public void removeBranchListener(IBranchListener branchListener) {
            try {
                this.listenerSemaphore.acquire();
                this.listenerSemaphore.drainPermits();
                this.branchListeners.remove(branchListener);
                this.listenerSemaphore.release();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        private void fireBranchStatusChangedEvent(BranchStatusChangedEvent event) {
            try {
                this.listenerSemaphore.acquire();
                this.listenerSemaphore.drainPermits();
                for (IBranchListener branchListener : this.branchListeners) {
                    branchListener.branchStatusChanged(event);
                }
                this.listenerSemaphore.release();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }
}
