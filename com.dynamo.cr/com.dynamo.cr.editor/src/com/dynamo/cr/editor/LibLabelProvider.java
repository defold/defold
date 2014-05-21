package com.dynamo.cr.editor;

import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IResource;
import org.eclipse.jface.viewers.IColorProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.widgets.Display;

public class LibLabelProvider extends LabelProvider implements IColorProvider {

    @Override
    public Image getImage(Object element) {
        if (element instanceof IFolder) {
            IFolder folder = (IFolder)element;
            if (folder.isLinked()) {
                return Activator.getDefault().getImageRegistry().get(Activator.LIBRARY_IMAGE_ID);
            }
        }
        return null;
    }

    @Override
    public String getText(Object element) {
        return null;
    }

    private boolean isLib(IResource resource) {
        if (resource == null || resource.getParent() == null || resource.getProject().equals(resource.getParent())) {
            return false;
        }
        if (resource instanceof IFolder) {
            IFolder folder = (IFolder)resource;
            if (folder.isLinked()) {
                return true;
            }
        }
        return isLib(resource.getParent());
    }

    @Override
    public Color getForeground(Object element) {
        if (element instanceof IResource) {
            if (isLib((IResource)element)) {
                return Display.getDefault().getSystemColor(SWT.COLOR_DARK_GRAY);
            }
        }
        return null;
    }

    @Override
    public Color getBackground(Object element) {
        return null;
    }

}
