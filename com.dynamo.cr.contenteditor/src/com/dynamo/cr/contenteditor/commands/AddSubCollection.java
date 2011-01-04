package com.dynamo.cr.contenteditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResource;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.dialogs.ListDialog;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.contenteditor.operations.AddSubCollectionOperation;

public class AddSubCollection extends AbstractHandler {

    private class CollectionSelectionDialog extends ResourceListSelectionDialog
    {
        public CollectionSelectionDialog(Shell parentShell, IContainer container)
        {
            super(parentShell, container, IResource.FILE | IResource.DEPTH_INFINITE);
            setTitle("Add Subcollection");
        }

        @Override
        protected String adjustPattern() {
            String text = super.adjustPattern();
            return text + ".collection";
        }
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editor = HandlerUtil.getActiveEditor(event);
        if (editor instanceof IEditor) {
            IFileEditorInput fi = (IFileEditorInput) editor.getEditorInput();
            CollectionSelectionDialog dialog = new CollectionSelectionDialog(PlatformUI.getWorkbench().getActiveWorkbenchWindow().getShell(), fi.getFile().getProject());
            int ret = dialog.open();

            if (ret == ListDialog.OK)
            {
                IResource r = (IResource) dialog.getResult()[0];

                AddSubCollectionOperation op = new AddSubCollectionOperation((IEditor) editor, (IFile) r);
                ((IEditor) editor).executeOperation(op);
            }
        }
        return null;
    }
}
