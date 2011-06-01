package com.dynamo.cr.editor.decorators;

import java.util.Vector;

import org.eclipse.core.resources.IMarker;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.IDecoration;
import org.eclipse.jface.viewers.ILabelProviderListener;
import org.eclipse.jface.viewers.ILightweightLabelDecorator;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.markers.BranchStatusMarker;

public class BranchStatusDecorator implements ILightweightLabelDecorator {

    private Vector<ILabelProviderListener> listeners;

    public BranchStatusDecorator() {
        this.listeners = new Vector<ILabelProviderListener>();
    }

    @Override
    public void addListener(ILabelProviderListener listener) {
        this.listeners.add(listener);
    }

    @Override
    public void dispose() {
    }

    @Override
    public boolean isLabelProperty(Object element, String property) {
        return property.equals(BranchStatusMarker.MARKER_ID);
    }

    @Override
    public void removeListener(ILabelProviderListener listener) {
        this.listeners.remove(listener);
    }

    @Override
    public void decorate(Object element, IDecoration decoration) {
        if (element instanceof IResource) {
            IResource resource = (IResource)element;
            if (resource.isAccessible()) {
                try {
                    IMarker[] markers = resource.findMarkers(BranchStatusMarker.MARKER_ID, true, 0);
                    String indexStatus = " ";
                    for (IMarker marker : markers) {
                        if (marker.exists()) {
                            Object indexStatusAttribute = marker.getAttribute(BranchStatusMarker.INDEX_STATUS_ATTRIBUTE_ID);
                            if (indexStatusAttribute != null && indexStatusAttribute instanceof String) {
                                indexStatus = (String)indexStatusAttribute;
                                break;
                            }
                        }
                    }
                    String workingTreeStatus = " ";
                    for (IMarker marker : markers) {
                        if (marker.exists()) {
                            Object workingTreeStatusAttribute = marker.getAttribute(BranchStatusMarker.WORKING_TREE_STATUS_ATTRIBUTE_ID);
                            if (workingTreeStatusAttribute != null && workingTreeStatusAttribute instanceof String) {
                                workingTreeStatus = (String)workingTreeStatusAttribute;
                                break;
                            }
                        }
                    }
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
                } catch (CoreException e) {
                    e.printStackTrace();
                }
            }
        }
    }

}
