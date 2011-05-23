package com.dynamo.cr.guieditor.commands;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.swt.dnd.Clipboard;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.dnd.GuiNodesTransfer;
import com.dynamo.cr.guieditor.operations.PasteGuiNodesOperation;
import com.dynamo.cr.guieditor.scene.BoxGuiNode;
import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.TextGuiNode;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.Type;

public class PasteGuiNodes extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof IGuiEditor) {
            IGuiEditor editor = (IGuiEditor) editorPart;

            Shell shell = editorPart.getSite().getShell();
            Clipboard cb = new Clipboard(shell.getDisplay());
            GuiNodesTransfer transfer = GuiNodesTransfer.getInstance();

            @SuppressWarnings("unchecked")
            List<NodeDesc> data = (List<NodeDesc>) cb.getContents(transfer);
            if (data != null) {
                List<GuiNode> nodes = new ArrayList<GuiNode>();
                for (NodeDesc nodeDesc : data) {
                    if (nodeDesc.getType() == Type.TYPE_BOX) {
                        BoxGuiNode node = new BoxGuiNode(editor.getScene(), nodeDesc);
                        nodes.add(node);
                    }
                    else if (nodeDesc.getType() == Type.TYPE_TEXT) {
                        TextGuiNode node = new TextGuiNode(editor.getScene(), nodeDesc);
                        nodes.add(node);
                    }
                }
                PasteGuiNodesOperation operation = new PasteGuiNodesOperation(editor, nodes);
                editor.executeOperation(operation);
            }
        }
        return null;
    }
}
