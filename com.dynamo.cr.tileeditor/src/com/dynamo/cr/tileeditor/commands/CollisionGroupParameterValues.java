package com.dynamo.cr.tileeditor.commands;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.commands.IParameterValues;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.PlatformUI;

import com.dynamo.cr.tileeditor.TileSetEditor2;
import com.dynamo.cr.tileeditor.scene.TileSetUtil;



public class CollisionGroupParameterValues implements IParameterValues {

    @SuppressWarnings({ "unchecked", "rawtypes" })
    @Override
    public Map getParameterValues() {
        final Map values = new HashMap();

        final IEditorPart editorPart = PlatformUI.getWorkbench().getActiveWorkbenchWindow().getActivePage().getActiveEditor();
        if (editorPart instanceof TileSetEditor2) {
            TileSetEditor2 editor = (TileSetEditor2)editorPart;
            List<String> collisionGroups = TileSetUtil.getCurrentCollisionGroupNames(editor.getPresenterContext());
            int n = collisionGroups.size();
            for (int i = 0; i < n; ++i) {
                values.put(collisionGroups.get(i), i);
            }
        }

        return values;
    }

}
