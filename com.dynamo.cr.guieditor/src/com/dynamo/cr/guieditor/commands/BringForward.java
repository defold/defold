package com.dynamo.cr.guieditor.commands;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.IHandler;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.operations.BringForwardOperation;
import com.dynamo.cr.guieditor.scene.GuiNode;

public class BringForward extends AbstractHandler implements IHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof IGuiEditor) {
            IGuiEditor editor = (IGuiEditor) editorPart;

            List<GuiNode> nodes = new ArrayList<GuiNode>();
            ISelection selection = HandlerUtil.getCurrentSelection(event);

            if (selection instanceof IStructuredSelection) {
                IStructuredSelection structuredSelection = (IStructuredSelection) selection;
                for (Object selected : structuredSelection.toList()) {
                    if (selected instanceof GuiNode) {
                        GuiNode node = (GuiNode) selected;
                        nodes.add(node);
                    }
                }
            }

            if (nodes.size() > 0) {
                BringForwardOperation operation = new BringForwardOperation(editor.getScene(), nodes);
                editor.executeOperation(operation);
            }
        }
        return null;
    }

}
