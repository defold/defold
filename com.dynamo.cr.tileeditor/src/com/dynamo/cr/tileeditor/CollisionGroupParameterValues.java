package com.dynamo.cr.tileeditor;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.commands.IParameterValues;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.PlatformUI;



public class CollisionGroupParameterValues implements IParameterValues {

    @SuppressWarnings({ "unchecked", "rawtypes" })
    @Override
    public Map getParameterValues() {
        final Map values = new HashMap();

        final IEditorPart editorPart = PlatformUI.getWorkbench().getActiveWorkbenchWindow().getActivePage().getActiveEditor();
        if (editorPart instanceof TileSetEditor) {
            TileSetEditor editor = (TileSetEditor)editorPart;
            List<String> collisionGroups = editor.getCollisionGroups();
            int n = collisionGroups.size();
            for (int i = 0; i < n; ++i) {
                values.put(collisionGroups.get(i), i);
            }
        }

        return values;
    }

}
