package com.dynamo.cr.go.ui.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.ide.IDE;
import org.eclipse.ui.statushandlers.StatusManager;

import com.dynamo.cr.go.Constants;
import com.dynamo.cr.go.core.CollectionInstanceNode;
import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.go.core.ComponentPropertyNode;
import com.dynamo.cr.go.core.GameObjectInstanceNode;
import com.dynamo.cr.go.core.RefComponentNode;
import com.dynamo.cr.sceneed.core.Node;

public class OpenReferenceHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        ISelection selection = HandlerUtil.getCurrentSelection(event);
        if (selection instanceof IStructuredSelection) {
            IStructuredSelection structuredSelection = (IStructuredSelection)selection;
            Object selected = structuredSelection.getFirstElement();
            Node node = null;
            String path = null;
            if (selected instanceof Node) {
                node = (Node)selected;
                // Nodes with no parent is the node already opened
                if (node.getParent() != null) {
                    if (selected instanceof RefComponentNode) {
                        path = ((RefComponentNode)selected).getComponent();
                    } else if (selected instanceof ComponentPropertyNode) {
                        path = ((ComponentPropertyNode)selected).getRefComponentNode().getComponent();
                    } else if (selected instanceof GameObjectInstanceNode) {
                        path = ((GameObjectInstanceNode)selected).getGameObject();
                    } else if (selected instanceof CollectionInstanceNode) {
                        path = ((CollectionInstanceNode)selected).getCollection();
                    } else if (selected instanceof CollectionNode) {
                        path = ((CollectionNode)selected).getPath();
                    }
                }
            }
            if (path != null) {
                IFile file = node.getModel().getFile(path);
                IWorkbenchPage page = PlatformUI.getWorkbench().getActiveWorkbenchWindow().getActivePage();
                try {
                    IDE.openEditor(page, file);
                } catch (PartInitException e1) {
                    Status status = new Status(IStatus.ERROR, Constants.PLUGIN_ID, e1.getMessage());
                    StatusManager.getManager().handle(status, StatusManager.LOG);
                }
            }
        }
        return null;
    }

}
