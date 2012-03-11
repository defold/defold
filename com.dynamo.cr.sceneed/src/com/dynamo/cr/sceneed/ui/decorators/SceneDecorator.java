package com.dynamo.cr.sceneed.ui.decorators;

import java.util.Vector;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.viewers.IDecoration;
import org.eclipse.jface.viewers.ILabelProviderListener;
import org.eclipse.jface.viewers.ILightweightLabelDecorator;

import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.Node;

public class SceneDecorator implements ILightweightLabelDecorator {

    public static final String DECORATOR_ID = "com.dynamo.cr.sceneed.ui.sceneDecorator";

    private Vector<ILabelProviderListener> listeners;

    public SceneDecorator() {
        this.listeners = new Vector<ILabelProviderListener>();
    }

    @Override
    public void dispose() {
    }

    @Override
    public void addListener(ILabelProviderListener listener) {
        this.listeners.add(listener);
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

        if (element instanceof Node) {
            Node node = (Node)element;
            IStatus status = node.getStatus();
            switch (status.getSeverity()) {
            case IStatus.INFO:
                decoration.addOverlay(getDescriptor(Activator.IMG_OVERLAY_INFO));
                break;
            case IStatus.WARNING:
                decoration.addOverlay(getDescriptor(Activator.IMG_OVERLAY_WARNING));
                break;
            case IStatus.ERROR:
                decoration.addOverlay(getDescriptor(Activator.IMG_OVERLAY_ERROR));
                break;
            }
        }
    }

    private ImageDescriptor getDescriptor(String id) {
        return Activator.getDefault().getImageRegistry().getDescriptor(id);
    }

}
