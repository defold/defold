package com.dynamo.cr.guieditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IResource;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.dialogs.ListDialog;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.ui.FilteredResourceListSelectionDialog;
import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.operations.AddTextureOperation;
import com.dynamo.cr.guieditor.scene.GuiScene;

public class AddTexture extends AbstractHandler {

    public void doExecute(IGuiEditor editor, IResource resource) {
        String resourcePath = EditorUtil.makeResourcePath(resource);

        GuiScene scene = editor.getScene();
        if (scene.getTextureFromPath(resourcePath) != null) {
            return;
        }

        String name = resource.getName();
        if (name.lastIndexOf('.') != -1) {
            name = name.substring(0, name.lastIndexOf('.'));
        }
        name = scene.getUniqueTextureName(name);
        AddTextureOperation operation = new AddTextureOperation(scene, name, resourcePath);
        editor.executeOperation(operation);
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof IGuiEditor) {
            IGuiEditor editor = (IGuiEditor) editorPart;
            IFileEditorInput input = (IFileEditorInput) editorPart.getEditorInput();
            IContainer contentRoot = EditorUtil.findContentRoot(input.getFile());
            FilteredResourceListSelectionDialog dialog = new FilteredResourceListSelectionDialog(editorPart.getSite().getShell(), contentRoot, IResource.FILE, new String[] {"jpg", "png"});
            int ret = dialog.open();
            if (ret == ListDialog.OK)
            {
                IResource r = (IResource) dialog.getResult()[0];
                doExecute(editor, r);
            }
            return null;
        }
        return null;
    }

}
