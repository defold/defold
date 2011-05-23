package com.dynamo.cr.guieditor.commands;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.swt.dnd.Clipboard;
import org.eclipse.swt.dnd.Transfer;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.dnd.GuiNodesTransfer;
import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.gui.proto.Gui.NodeDesc;

public class CopyGuiNodes extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof IGuiEditor) {
            IGuiEditor editor = (IGuiEditor) editorPart;

            List<GuiNode> nodes = editor.getSelectedNodes();
            if (nodes.size() > 0) {

                Shell shell = editorPart.getSite().getShell();
                Clipboard cb = new Clipboard(shell.getDisplay());

                GuiNodesTransfer transfer = GuiNodesTransfer.getInstance();
                List<NodeDesc> nodeDescs = new ArrayList<NodeDesc>();
                for (GuiNode node : nodes) {
                    nodeDescs.add(node.buildNodeDesc());
                }

                cb.setContents(new Object[] { nodeDescs }, new Transfer[] { transfer });
            }
        }
        return null;
    }
}
