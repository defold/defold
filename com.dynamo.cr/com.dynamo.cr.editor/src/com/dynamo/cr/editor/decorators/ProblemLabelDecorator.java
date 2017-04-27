package com.dynamo.cr.editor.decorators;

import org.eclipse.core.resources.IMarker;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.IDecoration;
import org.eclipse.jface.viewers.ILabelProviderListener;
import org.eclipse.jface.viewers.ILightweightLabelDecorator;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.editor.Activator;

public class ProblemLabelDecorator implements ILightweightLabelDecorator {

    private static Logger logger = LoggerFactory.getLogger(ProblemLabelDecorator.class);

    public ProblemLabelDecorator() {
    }

    @Override
    public void decorate(Object element, IDecoration decoration) {

        if (element instanceof IResource) {
            IResource resource = (IResource) element;
            if (resource.exists()) {
                try {
                    IMarker[] markers = resource.findMarkers(IMarker.PROBLEM, true, IResource.DEPTH_INFINITE);
                    if (markers.length > 0) {
                        for (IMarker marker : markers) {
                            if (marker.getAttribute(IMarker.SEVERITY, IMarker.SEVERITY_ERROR) == IMarker.SEVERITY_WARNING) {
                                decoration.addOverlay(Activator.getDefault().getImageRegistry().getDescriptor(Activator.OVERLAY_WARNING_IMAGE_ID));
                            } else {
                                decoration.addOverlay(Activator.getDefault().getImageRegistry().getDescriptor(Activator.OVERLAY_ERROR_IMAGE_ID));
                            }
                            if (marker.getResource() == resource) {
                            }
                        }
                    }
                } catch (CoreException e) {
                    logger.error("Error when decorating resource", e);
                }
            }
        }
    }

    @Override
    public void addListener(ILabelProviderListener listener) {

    }

    @Override
    public void dispose() {

    }

    @Override
    public boolean isLabelProperty(Object element, String property) {
        return false;
    }

    @Override
    public void removeListener(ILabelProviderListener listener) {

    }
}
