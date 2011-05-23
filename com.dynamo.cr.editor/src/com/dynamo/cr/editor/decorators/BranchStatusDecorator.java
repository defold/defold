package com.dynamo.cr.editor.decorators;

import java.util.Vector;

import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.QualifiedName;
import org.eclipse.jface.viewers.IDecoration;
import org.eclipse.jface.viewers.ILabelProviderListener;
import org.eclipse.jface.viewers.ILightweightLabelDecorator;

import com.dynamo.cr.editor.Activator;

public class BranchStatusDecorator implements ILightweightLabelDecorator {

    public static final QualifiedName BRANCH_STATUS_PROPERTY = new QualifiedName(Activator.PLUGIN_ID, "branchStatus");

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
        return property.equals(BRANCH_STATUS_PROPERTY.getLocalName());
    }

    @Override
    public void removeListener(ILabelProviderListener listener) {
        this.listeners.remove(listener);
    }

    @Override
    public void decorate(Object element, IDecoration decoration) {
        if (element instanceof IResource) {
            IResource resource = (IResource)element;
            try {
                String status = (String)resource.getSessionProperty(BRANCH_STATUS_PROPERTY);
                String imageKey = null;
                switch (status.charAt(0)) {
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
                decoration.addPrefix(String.format("[%s]", status));
            } catch (CoreException e) {
                e.printStackTrace();
            }
        }
    }

}
