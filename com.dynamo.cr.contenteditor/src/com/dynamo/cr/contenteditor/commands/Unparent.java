package com.dynamo.cr.contenteditor.commands;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.contenteditor.operations.UnparentOperation;
import com.dynamo.cr.contenteditor.scene.InstanceNode;
import com.dynamo.cr.contenteditor.scene.Node;

public class Unparent extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editor = HandlerUtil.getActiveEditor(event);
        if (editor instanceof IEditor) {
            Node[] selectedNodes = ((IEditor) editor).getSelectedNodes();
            List<InstanceNode> nodeList = new ArrayList<InstanceNode>();
            for (Node node : selectedNodes) {
                if (node instanceof InstanceNode) {
                    Node parent = node.getParent();
                    while (parent != null && !parent.acceptsChild(node)) {
                        parent = parent.getParent();
                    }
                    if (parent != null) {
                        nodeList.add((InstanceNode)node);
                    }
                }
            }
            InstanceNode[] nodes = new InstanceNode[nodeList.size()];
            nodes = nodeList.toArray(nodes);
            UnparentOperation op = new UnparentOperation(nodes);
            ((IEditor) editor).executeOperation(op);
        }
        return null;
    }

}
