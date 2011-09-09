package com.dynamo.cr.tileeditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.dialogs.IInputValidator;
import org.eclipse.jface.dialogs.InputDialog;
import org.eclipse.jface.window.Window;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.tileeditor.TileSetEditor;

public class RenameCollisionsGroup extends AbstractHandler {

    static class InputValidator implements IInputValidator {
        @Override
        public String isValid(String newText) {
            if (newText != null && !newText.equals("")) {
                return null;
            } else {
                return "The provided name can not be empty.";
            }
        }
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof TileSetEditor) {
            TileSetEditor tileSetEditor = (TileSetEditor)editorPart;
            String[] oldNames = tileSetEditor.getSelectedCollisionGroups();
            int n = oldNames.length;
            if (n > 0) {
                InputValidator validator = new InputValidator();
                String[] newNames = new String[n];
                for (int i = 0; i < n; ++i) {
                    InputDialog dialog = new InputDialog(HandlerUtil.getActiveShell(event), "Rename Collision Group", "Please specify the new name for the collision group.", oldNames[i], validator);
                    int result = dialog.open();
                    if (result == Window.OK) {
                        newNames[i] = dialog.getValue();
                    } else {
                        return null;
                    }
                }
                tileSetEditor.getPresenter().renameSelectedCollisionGroups(newNames);
            }
        }
        return null;
    }

}
