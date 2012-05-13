package com.dynamo.cr.tileeditor;

import org.eclipse.core.expressions.PropertyTester;

public class TileEditorPropertyTester extends PropertyTester {

    public TileEditorPropertyTester() {
        // TODO Auto-generated constructor stub
    }

    @Override
    public boolean test(Object receiver, String property, Object[] args,
            Object expectedValue) {
        if (receiver instanceof TileSetEditor) {
            TileSetEditor editor = (TileSetEditor)receiver;
            if (property.equals("isRenderingEnabled")) {
                return expectedValue.equals(editor.isRenderingEnabled());
            }
        } else if (receiver instanceof GridEditor) {
            GridEditor editor = (GridEditor)receiver;
            if (property.equals("isRenderingEnabled")) {
                return expectedValue.equals(editor.isRenderingEnabled());
            }
        }
        return false;
    }

}
