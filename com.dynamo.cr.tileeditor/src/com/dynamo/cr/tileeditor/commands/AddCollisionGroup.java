package com.dynamo.cr.tileeditor.commands;

import java.text.MessageFormat;
import java.util.List;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.tileeditor.TileSetEditor;

public class AddCollisionGroup extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof TileSetEditor) {
            TileSetEditor tileSetEditor = (TileSetEditor)editorPart;
            String name = "default";
            List<String> collisionGroups = tileSetEditor.getCollisionGroups();
            if (collisionGroups != null) {
                String pattern = name + "{0}";
                int i = 1;
                while (collisionGroups.contains(name)) {
                    name = MessageFormat.format(pattern, i++);
                }
            }
            tileSetEditor.getPresenter().addCollisionGroup(name);
        }
        return null;
    }

}
