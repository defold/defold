package com.dynamo.cr.guieditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IResource;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.dialogs.ListDialog;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.operations.AddTextureOperation;
import com.dynamo.cr.guieditor.scene.GuiScene;

public class AddTexture extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof IGuiEditor) {
            IGuiEditor editor = (IGuiEditor) editorPart;
            IFileEditorInput input = (IFileEditorInput) editorPart.getEditorInput();
            IContainer contentRoot = EditorUtil.findContentRoot(input.getFile());
            ResourceListSelectionDialog dialog = new ResourceListSelectionDialog(editorPart.getSite().getShell(), contentRoot, IResource.FILE | IResource.DEPTH_INFINITE);
            int ret = dialog.open();
            if (ret == ListDialog.OK)
            {
                IResource r = (IResource) dialog.getResult()[0];
                org.eclipse.core.runtime.IPath fullPath = r.getFullPath();
                String relativePath = fullPath.makeRelativeTo(contentRoot.getFullPath()).toPortableString();

                GuiScene scene = editor.getScene();
                if (scene.getTextureFromPath(relativePath) != null) {
                    return null;
                }

                String name = r.getName();
                if (name.lastIndexOf('.') != -1) {
                    name = name.substring(0, name.lastIndexOf('.'));
                }
                name = scene.getUniqueTextureName(name);
                AddTextureOperation operation = new AddTextureOperation(scene, name, relativePath);
                editor.executeOperation(operation);
            }
            return null;
        }
        return null;
    }

}
