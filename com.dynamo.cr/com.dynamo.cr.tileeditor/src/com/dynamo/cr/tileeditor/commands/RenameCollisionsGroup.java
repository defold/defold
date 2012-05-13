package com.dynamo.cr.tileeditor.commands;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.dialogs.IInputValidator;
import org.eclipse.jface.dialogs.InputDialog;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.window.Window;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.tileeditor.TileSetEditor;
import com.dynamo.cr.tileeditor.TileSetEditorOutlinePage.CollisionGroupItem;

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
        ISelection selection = HandlerUtil.getCurrentSelection(event);
        if (editorPart instanceof TileSetEditor && selection instanceof IStructuredSelection) {
            TileSetEditor tileSetEditor = (TileSetEditor)editorPart;
            IStructuredSelection structuredSelection = (IStructuredSelection)selection;
            Object[] items = structuredSelection.toArray();
            List<String> oldNames = new ArrayList<String>();
            for (Object item : items) {
                if (item instanceof CollisionGroupItem) {
                    oldNames.add(((CollisionGroupItem)item).group);
                }
            }
            int n = oldNames.size();
            if (n > 0) {
                InputValidator validator = new InputValidator();
                String[] newNames = new String[n];
                for (int i = 0; i < n; ++i) {
                    InputDialog dialog = new InputDialog(HandlerUtil.getActiveShell(event), "Rename Collision Group", "Please specify the new name for the collision group.", oldNames.get(i), validator);
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
