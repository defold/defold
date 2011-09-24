package com.dynamo.cr.goeditor;

import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.dialogs.ListDialog;

import com.dynamo.cr.editor.core.IResourceType;

public class EmbeddedComponentListDialog extends ListDialog {

    public EmbeddedComponentListDialog(Shell parent, final ImageFactory imageFactory, IResourceType[] types) {
        super(parent);

        setTitle("Add Component");
        setMessage("Select component type from list:");
        setContentProvider(new ArrayContentProvider());
        setInput(types);
        setLabelProvider(new LabelProvider() {
            @Override
            public Image getImage(Object element) {
                IResourceType resourceType = (IResourceType) element;
                return imageFactory.getImageFromFilename("dummy." + resourceType.getFileExtension());
            }

            @Override
            public String getText(Object element) {
                IResourceType resourceType = (IResourceType) element;
                return resourceType.getName();
            }
        });
    }
}
