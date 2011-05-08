package com.dynamo.cr.editor.decorators;

import java.util.Vector;

import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.QualifiedName;
import org.eclipse.jface.viewers.IDecoration;
import org.eclipse.jface.viewers.ILabelProviderListener;
import org.eclipse.jface.viewers.ILightweightLabelDecorator;

import com.dynamo.cr.editor.Activator;

public class GitDecorator implements ILightweightLabelDecorator {

    public static final QualifiedName GIT_INDEX_STATUS_PROPERTY = new QualifiedName("git", "index_status");

    private Vector<ILabelProviderListener> listeners;

    public GitDecorator() {
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
        return property.equals(GIT_INDEX_STATUS_PROPERTY.getLocalName());
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
                String status = (String)resource.getSessionProperty(GIT_INDEX_STATUS_PROPERTY);
                if (status.equals("M")) {
                    decoration.addOverlay(Activator.getImageDescriptor("/icons/overlay_edit.png"));
                } else if (status.equals("A")) {
                    decoration.addOverlay(Activator.getImageDescriptor("/icons/overlay_add.png"));
                } else if (status.equals("D")) {
                    decoration.addOverlay(Activator.getImageDescriptor("/icons/overlay_delete.png"));
                } else if (status.equals("R")) {
                    decoration.addOverlay(Activator.getImageDescriptor("/icons/overlay_edit.png"));
                } else if (status.equals("C")) {
                    decoration.addOverlay(Activator.getImageDescriptor("/icons/overlay_add.png"));
                } else if (status.equals("U")) {
                    decoration.addOverlay(Activator.getImageDescriptor("/icons/overlay_error.png"));
                }
            } catch (CoreException e) {
                e.printStackTrace();
            }
        }
    }

}
