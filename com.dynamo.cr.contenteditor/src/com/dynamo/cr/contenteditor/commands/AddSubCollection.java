package com.dynamo.cr.contenteditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.dialogs.ListDialog;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.scene.graph.CollectionInstanceNode;
import com.dynamo.cr.scene.graph.CollectionNode;
import com.dynamo.cr.scene.graph.INodeFactory;
import com.dynamo.cr.scene.graph.Node;
import com.dynamo.cr.scene.graph.Scene;
import com.dynamo.cr.scene.operations.AddSubCollectionOperation;
import com.dynamo.cr.scene.resource.Resource;

public class AddSubCollection extends AbstractHandler {

    private class CollectionSelectionDialog extends ResourceListSelectionDialog
    {
        public CollectionSelectionDialog(Shell parentShell, IContainer container)
        {
            super(parentShell, container, IResource.FILE | IResource.DEPTH_INFINITE);
            setTitle("Add Subcollection");
        }

        @Override
        protected String adjustPattern() {
            String text = super.adjustPattern();
            return text + ".collection";
        }
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof IEditor) {
            IFileEditorInput fi = (IFileEditorInput) editorPart.getEditorInput();
            IEditor editor = (IEditor)editorPart;
            CollectionSelectionDialog dialog = new CollectionSelectionDialog(PlatformUI.getWorkbench().getActiveWorkbenchWindow().getShell(), fi.getFile().getProject());
            int ret = dialog.open();

            if (ret == ListDialog.OK)
            {
                IResource r = (IResource) dialog.getResult()[0];
                Node root = editor.getRoot();
                Scene scene = editor.getScene();

                INodeFactory factory = editor.getNodeFactory();
                String name = EditorUtil.makeResourcePath(r);

                try {
                    Resource resource = editor.getResourceFactory().load(new NullProgressMonitor(), name);
                    CollectionNode proto = (CollectionNode) factory.create(name, resource, root, scene);
                    CollectionNode parent = (CollectionNode)root;
                    CollectionInstanceNode node = new CollectionInstanceNode(r.getName(), scene, name, proto);
                    ((IEditor) editor).executeOperation(new AddSubCollectionOperation(node, parent));
                } catch (Exception e) {
                    throw new ExecutionException(e.getMessage(), e);
                }
            }
        }
        return null;
    }
}
