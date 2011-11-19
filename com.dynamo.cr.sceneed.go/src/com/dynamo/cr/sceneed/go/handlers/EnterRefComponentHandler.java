package com.dynamo.cr.sceneed.go.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.ide.IDE;
import org.eclipse.ui.statushandlers.StatusManager;

import com.dynamo.cr.sceneed.go.RefComponentNode;
import com.dynamo.cr.sceneed.ui.Activator;

public class EnterRefComponentHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        ISelection selection = HandlerUtil.getCurrentSelection(event);
        if (selection instanceof IStructuredSelection) {
            IStructuredSelection structuredSelection = (IStructuredSelection)selection;
            RefComponentNode node = (RefComponentNode)structuredSelection.getFirstElement();
            IFile file = node.getModel().getContentRoot().getFile(new Path(node.getComponent()));
            IWorkbenchPage page = PlatformUI.getWorkbench().getActiveWorkbenchWindow().getActivePage();
            try {
                IDE.openEditor(page, file);
            } catch (PartInitException e1) {
                Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, e1.getMessage());
                StatusManager.getManager().handle(status, StatusManager.LOG);
            }
        }
        return null;
    }

}
