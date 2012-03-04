package com.dynamo.cr.editor.decorators;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.Vector;

import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IDecoration;
import org.eclipse.jface.viewers.ILabelProviderListener;
import org.eclipse.jface.viewers.ILightweightLabelDecorator;
import org.eclipse.jface.viewers.LabelProviderChangedEvent;
import org.eclipse.swt.widgets.Display;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.statushandlers.StatusManager;

import com.dynamo.cr.client.BranchStatusChangedEvent;
import com.dynamo.cr.client.IBranchListener;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.compare.ResourceStatus;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.services.IBranchService;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;

public class BranchStatusDecorator implements ILightweightLabelDecorator, IBranchListener {

    private Vector<ILabelProviderListener> listeners;
    private Map<IResource, BranchStatus.Status> resourceToStatus = new HashMap<IResource, BranchStatus.Status>();

    public BranchStatusDecorator() {
        this.listeners = new Vector<ILabelProviderListener>();

        IBranchService branchService = (IBranchService)PlatformUI.getWorkbench().getService(IBranchService.class);
        if (branchService != null) {
            branchService.addBranchListener(this);
        } else {
            Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, "Unable to locate IBranchService");
            StatusManager.getManager().handle(status, StatusManager.LOG);
        }
    }

    @Override
    public void addListener(ILabelProviderListener listener) {
        this.listeners.add(listener);
    }

    @Override
    public void dispose() {
        IBranchService branchService = (IBranchService)PlatformUI.getWorkbench().getService(IBranchService.class);
        if (branchService != null) {
            branchService.removeBranchListener(this);
        } else {
            Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, "Unable to locate IBranchService");
            StatusManager.getManager().handle(status, StatusManager.LOG);
        }
    }

    @Override
    public boolean isLabelProperty(Object element, String property) {
        return false;
    }

    @Override
    public void removeListener(ILabelProviderListener listener) {
        this.listeners.remove(listener);
    }

    @Override
    public void decorate(Object element, IDecoration decoration) {

        IResource resource = null;
        if (element instanceof IResource) {
            resource = (IResource) element;

        } else if (element instanceof ResourceStatus) {
            ResourceStatus resourceStatus = (ResourceStatus)element;
            resource = resourceStatus.getResource();
        }

        if (resource != null && resourceToStatus.containsKey(resource)) {
            BranchStatus.Status fileStatus = resourceToStatus.get(resource);
            doDecorate(decoration, fileStatus.getIndexStatus(), fileStatus.getWorkingTreeStatus());
        }
    }

    private void doDecorate(IDecoration decoration, String indexStatus, String workingTreeStatus) {
        String imageKey = null;
        switch (indexStatus.charAt(0)) {
        case 'M': // modified
        case 'R': // renamed
            imageKey = Activator.OVERLAY_EDIT_IMAGE_ID;
            break;
        case 'A': // added
        case 'C': // copied
            imageKey = Activator.OVERLAY_ADD_IMAGE_ID;
            break;
        case 'D': // deleted
            imageKey = Activator.OVERLAY_DELETE_IMAGE_ID;
            break;
        case 'U': // unmerged
            imageKey = Activator.OVERLAY_ERROR_IMAGE_ID;
            break;
        }
        if (imageKey != null) {
            decoration.addOverlay(Activator.getDefault().getImageRegistry().getDescriptor(imageKey));
        }
        String status = (indexStatus + workingTreeStatus).trim();
        if (!status.equals("")) {
            decoration.addPrefix(String.format("[%s]", status));
        }
    }

    @Override
    public void branchStatusChanged(BranchStatusChangedEvent event) {
        IProject project = Activator.getDefault().getProject();
        // TODO: Is it possible to avoid this fix
        // project can be null while switching branches.
        // Should have a more well defined semantics
        if (project == null)
            return;

        IFolder contentRoot = EditorUtil.getContentRoot(project);

        Set<IResource> changedResources = new HashSet<IResource>();
        changedResources.addAll(resourceToStatus.keySet());

        BranchStatus branchStatus = event.getBranchStatus();
        List<BranchStatus.Status> fileStatusList = branchStatus.getFileStatusList();
        resourceToStatus.clear();
        for (BranchStatus.Status fileStatus : fileStatusList) {
            IResource resource = contentRoot.findMember(fileStatus.getName());
            if (resource != null) {
                resourceToStatus.put(resource, fileStatus);
            } else {
                Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, "Unable to locate " + fileStatus.getName());
                StatusManager.getManager().handle(status, StatusManager.LOG);
            }
        }
        changedResources.addAll(resourceToStatus.keySet());

        LabelProviderChangedEvent providerEvent = new LabelProviderChangedEvent(this, changedResources.toArray());
        fireLabelProviderChanged(providerEvent);
    }

    private void fireLabelProviderChanged(final LabelProviderChangedEvent event) {
        // Decorate using current UI thread
        Display.getDefault().asyncExec(new Runnable() {
            @Override
            public void run()
            {
                for (ILabelProviderListener listener : listeners) {
                    listener.labelProviderChanged(event);
                }
            }
        });
    }
}
