package com.dynamo.cr.contenteditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.contenteditor.editors.NodeLoaderFactory;
import com.dynamo.cr.contenteditor.scene.InstanceNode;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.PrototypeNode;
import com.dynamo.cr.contenteditor.scene.Scene;

public class AddGameObjectOperation extends AbstractOperation {

    private IEditor editor;
    private InstanceNode node;
    private IFile file;

    public AddGameObjectOperation(IEditor editor, IFile file) {
        super("Add GameObject");
        this.editor = editor;
        this.file= file;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {

        Node root = editor.getRoot();
        Scene scene = editor.getScene();

        NodeLoaderFactory factory = editor.getLoaderFactory();
        try {
            IContainer content_root = factory.getContentRoot();
            PrototypeNode proto = (PrototypeNode) factory.load(new NullProgressMonitor(), scene, file.getFullPath().toPortableString());
            node = new InstanceNode(scene, scene.getUniqueId(file.getName()), file.getFullPath().makeRelativeTo(content_root.getFullPath()).toPortableString(), proto);
            root.addNode(node);
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
        Node root = editor.getRoot();
        root.removeNode(node);
        return Status.OK_STATUS;
    }

}
