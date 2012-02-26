package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.swt.dnd.Transfer;

import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenter;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;
import com.dynamo.cr.sceneed.core.operations.RemoveChildrenOperation;
import com.dynamo.cr.sceneed.core.util.NodeListTransfer;
import com.google.inject.Inject;
import com.google.protobuf.Message;

public class ScenePresenter implements IPresenter, IModelListener {

    private final ISceneModel model;
    private final ISceneView view;
    private final INodeTypeRegistry nodeTypeRegistry;
    private final ILoaderContext loaderContext;
    private final IClipboard clipboard;
    private ManipulatorController manipulatorController;

    @Inject
    public ScenePresenter(ISceneModel model, ISceneView view, INodeTypeRegistry manager, ILoaderContext loaderContext, IClipboard clipboard, ManipulatorController manipulatorController) {
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
    public void onSelectAll() {
        IStructuredSelection oldSelection = this.model.getSelection();
        IStructuredSelection selection = new StructuredSelection(this.model.getRoot().getChildren());
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
        INodeLoader<Node> loader = nodeType.getLoader();
        try {
            Node node = loader.load(this.loaderContext, contents);
            this.model.setRoot(node);
        } catch (Exception e) {
            // Should never happen in production
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, "Node could not be loaded", e));
        }
    }

    @Override
    public void onSave(OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException {
        Node node = this.model.getRoot();
        INodeType nodeType = this.nodeTypeRegistry.getNodeTypeClass(node.getClass());
        INodeLoader<Node> loader = nodeType.getLoader();
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

    @Override
    public void onCopySelection(IPresenterContext presenterContext, ILoaderContext loaderContext, IProgressMonitor monitor) throws IOException, CoreException {
        IStructuredSelection selection = presenterContext.getSelection();
        Object[] objects = selection.toArray();
        List<Node> nodes = new ArrayList<Node>(objects.length);
        for (Object object : objects) {
            nodes.add((Node)object);
        }
        if (nodes.size() > 0) {
            NodeListTransfer transfer = NodeListTransfer.getInstance();
            this.clipboard.setContents(new Object[] {nodes}, new Transfer[] {transfer});
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
            NodeListTransfer transfer = NodeListTransfer.getInstance();
            this.clipboard.setContents(new Object[] {nodes}, new Transfer[] {transfer});
            presenterContext.executeOperation(new RemoveChildrenOperation(nodes, presenterContext));
        }
    }

    @SuppressWarnings("unchecked")
    @Override
    public void onPasteIntoSelection(IPresenterContext context) throws IOException, CoreException {
        Object[] selection = context.getSelection().toArray();
        if (selection.length == 0) {
            return;
        }

        NodeListTransfer transfer = NodeListTransfer.getInstance();
        List<Node> nodes = (List<Node>)this.clipboard.getContents(transfer);

        if (nodes != null && nodes.size() > 0) {
            Node target = (Node)selection[0];
            INodeType targetType = null;
            // Verify acceptance of child classes
            while (target != null) {
                boolean accepted = true;
                targetType = this.nodeTypeRegistry.getNodeTypeClass(target.getClass());
                if (targetType != null) {
                    for (Node node : nodes) {
                        boolean nodeAccepted = false;
                        for (INodeType nodeType : targetType.getReferenceNodeTypes()) {
                            if (nodeType.getNodeClass().isAssignableFrom(node.getClass())) {
                                nodeAccepted = true;
                                break;
                            }
                        }
                        if (!nodeAccepted) {
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
            context.executeOperation(new AddChildrenOperation("Paste", target, nodes, context));
        }
    }
}
