package com.dynamo.cr.contenteditor.actions;

import org.eclipse.jface.action.Action;

import com.dynamo.cr.contenteditor.editors.IEditor;

public abstract class EditorAction extends Action {

    protected IEditor editor;

    public EditorAction(String text, int style) {
        super(text, style);
    }

    public abstract void setActiveEditor(IEditor targetEditor);

}
