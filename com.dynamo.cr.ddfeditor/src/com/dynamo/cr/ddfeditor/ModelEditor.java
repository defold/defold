package com.dynamo.cr.ddfeditor;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.IPath;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.dialogs.ListDialog;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;

import com.dynamo.model.proto.Model.ModelDesc;

public class ModelEditor extends DdfEditor {

    public ModelEditor() {
        super(ModelDesc.newBuilder().buildPartial());
    }

    @Override
    public boolean hasNewValueForPath(String path) {
        return path.equalsIgnoreCase("/textures");
    }

    private class CollectionSelectionDialog extends ResourceListSelectionDialog
    {
        public CollectionSelectionDialog(Shell parentShell, IContainer container)
        {
            super(parentShell, container, IResource.FILE | IResource.DEPTH_INFINITE);
            setTitle("Add textures...");
        }
    }

    @Override
    public Object getNewValueForPath(String path) {

        if (path.equalsIgnoreCase("/textures")) {

            IFileEditorInput fi = (IFileEditorInput) getEditorInput();
            IContainer contentRoot = findContentRoot(fi.getFile());
            if (contentRoot == null) {
                return null;
            }

            ResourceListSelectionDialog dialog = new CollectionSelectionDialog(PlatformUI.getWorkbench().getActiveWorkbenchWindow().getShell(), fi.getFile().getProject());
            int ret = dialog.open();

            if (ret == ListDialog.OK)
            {
                IResource r = (IResource) dialog.getResult()[0];
                IPath fullPath = r.getFullPath();
                return fullPath.makeRelativeTo(contentRoot.getFullPath()).toPortableString();
            }

            return null;
        }
        return null;
    }
}
