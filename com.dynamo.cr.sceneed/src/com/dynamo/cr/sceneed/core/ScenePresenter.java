package com.dynamo.cr.sceneed.core;

import java.io.CharArrayReader;
import java.io.CharArrayWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

import org.apache.commons.io.input.ReaderInputStream;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.swt.dnd.Transfer;

import com.dynamo.cr.editor.core.ILogger;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenter;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;
import com.dynamo.cr.sceneed.core.operations.RemoveChildrenOperation;
import com.dynamo.cr.sceneed.core.util.NodeTransfer;
import com.google.inject.Inject;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class ScenePresenter implements IPresenter, IModelListener {

    private final ISceneModel model;
    private final ISceneView view;
    private final INodeTypeRegistry nodeTypeRegistry;
    private final ILoaderContext loaderContext;
    private final IClipboard clipboard;
    private ManipulatorController manipulatorController;

    @Inject
    public ScenePresenter(ISceneModel model, ISceneView view, INodeTypeRegistry manager, ILogger logger, ILoaderContext loaderContext, IClipboard clipboard, ManipulatorController manipulatorController) {
        this.model = model;
        this.view = view;
        this.nodeTypeRegistry = manager;
        this.loaderContext = loaderContext;
        this.clipboard = clipboard;
        this.manipulatorController = manipulatorController;
    }

    @Override
    public void onSelect(IStructuredSelection selection) {
        IStructuredSelection oldSelection = this.model.getSelection();
        if (!oldSelection.toList().equals(selection.toList())) {
            this.model.setSelection(selection);
            this.view.refresh(selection, this.model.isDirty());
        }
    }

    @Override
    public void onRefresh() {
        this.view.setRoot(this.model.getRoot());
        this.view.refresh(this.model.getSelection(), this.model.isDirty());
    }

    @Override
    public final void onLoad(String type, InputStream contents) throws IOException, CoreException {
        INodeType nodeType = this.nodeTypeRegistry.getNodeTypeFromExtension(type);
        INodeLoader loader = nodeType.getLoader();
        @SuppressWarnings("unchecked")
        Node node = loader.load(this.loaderContext, (Class<? extends Node>)nodeType.getNodeClass(), contents);
        this.model.setRoot(node);
    }

    @Override
    public void onSave(OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException {
        Node node = this.model.getRoot();
        INodeType nodeType = this.nodeTypeRegistry.getNodeTypeClass(node.getClass());
        INodeLoader loader = nodeType.getLoader();
        Message message = loader.buildMessage(this.loaderContext, node, monitor);
        SceneUtil.saveMessage(message, contents, monitor);
        this.model.clearDirty();
    }

    @Override
    public void onResourceChanged(IResourceChangeEvent event) throws CoreException {
        this.model.handleResourceChanged(event);
    }

    @Override
    public void rootChanged(Node root) {
        this.view.setRoot(root);
    }

    @Override
    public void stateChanged(IStructuredSelection selection, boolean dirty) {
        this.view.refresh(selection, dirty);
        this.manipulatorController.refresh();
    }

    private static class NodeData {
        public Class<? extends Node> type;
        public char[] data;
        @SuppressWarnings("unchecked")
        private void readObject(java.io.ObjectInputStream stream)
                throws IOException, ClassNotFoundException {
            this.type = (Class<? extends Node>)stream.readObject();
            int dataSize = stream.readInt();
            byte[] bytes = new byte[dataSize];
            stream.read(bytes, 0, dataSize);
            this.data = new String(bytes).toCharArray();
        }
        private void writeObject(java.io.ObjectOutputStream stream)
            throws IOException {
            stream.writeObject(this.type);
            stream.writeInt(this.data.length);
            stream.write(new String(this.data).getBytes());
        }
    }

    @Override
    public void onCopySelection(IPresenterContext presenterContext, ILoaderContext loaderContext, IProgressMonitor monitor) throws IOException, CoreException {
        IStructuredSelection selection = presenterContext.getSelection();
        Object[] objects = selection.toArray();
        List<Node> nodes = new ArrayList<Node>(objects.length);
        for (Object object : objects) {
            nodes.add((Node)object);
        }
        if (nodes.size() > 0) {
            List<NodeData> data = new ArrayList<NodeData>();
            NodeTransfer transfer = NodeTransfer.getInstance();
            Node parent = nodes.get(0).getParent();
            INodeLoader parentLoader = this.nodeTypeRegistry.getNodeTypeClass(parent.getClass()).getLoader();
            for (Node node : nodes) {
                Message message = null;
                if (parentLoader != null) {
                    message = parentLoader.buildMessage(loaderContext, node, monitor);
                } else {
                    INodeType nodeType = this.nodeTypeRegistry.getNodeTypeClass(node.getClass());
                    if (nodeType != null) {
                        INodeLoader loader = nodeType.getLoader();
                        message = loader.buildMessage(loaderContext, node, monitor);
                    }
                }
                NodeData entry = new NodeData();
                entry.type = node.getClass();
                CharArrayWriter writer = new CharArrayWriter();
                TextFormat.print(message, writer);
                entry.data = writer.toCharArray();
                data.add(entry);
            }
            this.clipboard.setContents(new Object[] {data}, new Transfer[] {transfer});
        }
    }

    @Override
    public void onCutSelection(IPresenterContext presenterContext, ILoaderContext loaderContext, IProgressMonitor monitor) throws IOException, CoreException {
        IStructuredSelection selection = presenterContext.getSelection();
        Object[] objects = selection.toArray();
        List<Node> nodes = new ArrayList<Node>(objects.length);
        for (Object object : objects) {
            nodes.add((Node)object);
        }
        if (nodes.size() > 0) {
            List<NodeData> data = new ArrayList<NodeData>();
            NodeTransfer transfer = NodeTransfer.getInstance();
            Node parent = nodes.get(0).getParent();
            INodeLoader parentLoader = this.nodeTypeRegistry.getNodeTypeClass(parent.getClass()).getLoader();
            for (Node node : nodes) {
                Message message = null;
                if (parentLoader != null) {
                    message = parentLoader.buildMessage(loaderContext, node, monitor);
                } else {
                    INodeType nodeType = this.nodeTypeRegistry.getNodeTypeClass(node.getClass());
                    if (nodeType != null) {
                        INodeLoader loader = nodeType.getLoader();
                        message = loader.buildMessage(loaderContext, node, monitor);
                    }
                }
                NodeData entry = new NodeData();
                entry.type = node.getClass();
                CharArrayWriter writer = new CharArrayWriter();
                TextFormat.print(message, writer);
                entry.data = writer.toCharArray();
                data.add(entry);
            }
            presenterContext.executeOperation(new RemoveChildrenOperation(nodes, presenterContext));
            this.clipboard.setContents(new Object[] {data}, new Transfer[] {transfer});
        }
    }

    @Override
    public void onPasteIntoSelection(IPresenterContext context) throws IOException, CoreException {
        Object[] selection = context.getSelection().toArray();
        if (selection.length != 1) {
            return;
        }
        NodeTransfer transfer = NodeTransfer.getInstance();

        @SuppressWarnings("unchecked")
        List<NodeData> data = (List<NodeData>) this.clipboard.getContents(transfer);
        if (data != null && data.size() > 0) {
            Node target = (Node)selection[0];
            INodeType targetType = null;
            // Verify acceptance of child classes
            while (target != null) {
                boolean accepted = true;
                targetType = this.nodeTypeRegistry.getNodeTypeClass(target.getClass());
                if (targetType != null) {
                    for (NodeData entry : data) {
                        if (!targetType.getChildClasses().contains(entry.type)) {
                            accepted = false;
                            break;
                        }
                    }
                    if (accepted) {
                        break;
                    }
                }
                target = target.getParent();
            }
            if (target == null || targetType == null)
                return;
            INodeLoader targetLoader = targetType.getLoader();
            List<Node> nodes = new ArrayList<Node>(data.size());
            for (NodeData entry : data) {
                CharArrayReader r = new CharArrayReader(entry.data);
                ReaderInputStream contents = new ReaderInputStream(r);
                Node node = targetLoader.load(loaderContext, entry.type, contents);
                nodes.add(node);
            }
            context.executeOperation(new AddChildrenOperation("Paste", target, nodes, context));
        }
    }
}
