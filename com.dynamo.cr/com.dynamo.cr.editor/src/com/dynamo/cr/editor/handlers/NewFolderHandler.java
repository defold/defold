package com.dynamo.cr.editor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.dialogs.IInputValidator;
import org.eclipse.jface.dialogs.InputDialog;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.editor.core.EditorUtil;

public class NewFolderHandler extends AbstractHandler {

    class Validator implements IInputValidator {

        private IContainer root;

        public Validator(IContainer root) {
            this.root = root;
        }

        @Override
        public String isValid(String newText) {
            if (newText.length() > 0) {
                IFolder newFolder = root.getFolder(new Path(newText));
                if (newFolder.exists()) {
                    return String.format("File or folder '%s' already exists", newText);
                }
            }
            return null;
        }
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        ISelection selection = HandlerUtil.getActiveSite(event)
                .getSelectionProvider().getSelection();

        IProject project = EditorUtil.getProject();
        if (project == null) {
            return null;
        }

        IContainer root = null;

        /*
         * First try to find directory based on selection
         */
        if (!selection.isEmpty() && selection instanceof IStructuredSelection) {
            IStructuredSelection structSelection = (IStructuredSelection) selection;
            Object first = structSelection.getFirstElement();
            if (first instanceof IResource) {
                if (first instanceof IContainer) {
                    root = (IContainer) first;
                } else {
                    root = ((IResource) first).getParent();
                }
            }
        }

        /*
         * Fallback to content-root (when using the command without selection)
         */
        if (root == null) {
            root = EditorUtil.getContentRoot(project);
        }

        InputDialog dlg = new InputDialog(HandlerUtil.getActiveShell(event), "New Folder", "New folder name", "", new Validator(root));

        if (dlg.open() == Dialog.OK) {
            IFolder newFolder = root.getFolder(new Path(dlg.getValue()));
            try {
                /*
                 * No undo for this operation.
                 * The following bug https://bugs.eclipse.org/bugs/show_bug.cgi?id=219901
                 * is mentioned in WizardNewFolderMainPage#createNewFolder so we skip undo here as well.
                 * In other words we do not use the CreateFolderOperation
                 */
                newFolder.create(true, true, new NullProgressMonitor());
            } catch (CoreException e) {
                throw new ExecutionException("Failed to created new folder", e);
            }
        }
        return null;
    }

}
