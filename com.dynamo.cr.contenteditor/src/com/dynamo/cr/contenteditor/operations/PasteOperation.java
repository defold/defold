package com.dynamo.cr.contenteditor.operations;

import java.io.ByteArrayInputStream;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.contenteditor.editors.ResourceLoaderFactory;
import com.dynamo.cr.contenteditor.scene.CollectionNode;
import com.dynamo.cr.contenteditor.scene.InstanceNode;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.Scene;

public class PasteOperation extends AbstractOperation {

    private String data;
    private IEditor editor;
    private List<Node> addedNodes;
    private Node pasteTarget;

    public void setSceneAndUniqueIdentifier(Scene scene, Node node) {
        if (node instanceof InstanceNode) {
            InstanceNode in = (InstanceNode) node;
            String id = scene.getUniqueId(in.getIdentifier());
            // NOTE: We must scene to the node here in order to ensure unique identifiers
            node.setScene(scene);
            node.setIdentifier(id);
        }
        node.setScene(scene);
        for (Node n : node.getChilden()) {
            setSceneAndUniqueIdentifier(scene, n);
        }
    }

    public PasteOperation(IEditor editor, String data, Node paste_target) {
        super("Paste");
        this.editor = editor;
        this.data = data;
        this.pasteTarget = paste_target;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {

        addedNodes = new ArrayList<Node>();
        ResourceLoaderFactory factory = editor.getLoaderFactory();

        ByteArrayInputStream stream = new ByteArrayInputStream(data.getBytes());
        Scene scene = new Scene();
        try {
            CollectionNode node = (CollectionNode) factory.load(new NullProgressMonitor(), scene, "clipboard.collection", stream);
            setSceneAndUniqueIdentifier(pasteTarget.getScene(), node);
            for (Node n : node.getChilden()) {
                pasteTarget.addNode(n);
                addedNodes.add(n);
            }
        } catch (Throwable e) {
            throw new ExecutionException(e.getMessage(), e);
        }

        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        return execute(monitor, info);
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        for (Node n : addedNodes) {
            pasteTarget.removeNode(n);
        }
        return Status.OK_STATUS;
    }

}
