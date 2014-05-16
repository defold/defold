package com.dynamo.cr.editor;

import org.eclipse.core.resources.IFolder;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.swt.graphics.Image;

public class LibLabelProvider extends LabelProvider {

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

}
