package com.dynamo.cr.contenteditor.commands;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.contenteditor.operations.ParentOperation;
import com.dynamo.cr.contenteditor.scene.InstanceNode;
import com.dynamo.cr.contenteditor.scene.Node;

public class Parent extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editor = HandlerUtil.getActiveEditor(event);
        if (editor instanceof IEditor) {
            Node[] selectedNodes = ((IEditor) editor).getSelectedNodes();
            InstanceNode parent = null;
            List<InstanceNode> list = new ArrayList<InstanceNode>();
            for (Node node : selectedNodes) {
                if (node instanceof InstanceNode) {
                    InstanceNode instanceNode = (InstanceNode)node;
                    if (parent == null) {
                        parent = instanceNode;
                    } else {
                        list.add(instanceNode);
                    }
                }
            }
            if (parent != null && list.size() > 0) {
                InstanceNode[] nodes = new InstanceNode[list.size()];
                nodes = list.toArray(nodes);
                ParentOperation op = new ParentOperation(nodes, parent);
                ((IEditor) editor).executeOperation(op);
            }
        }
        return null;
    }
}
