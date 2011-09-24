package com.dynamo.cr.goeditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.goeditor.GameObjectEditor2;

public class AddEmbeddedComponent extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        GameObjectEditor2 editor = (GameObjectEditor2) editorPart;
        editor.getPresenter().onAddEmbeddedComponent();
        return null;
    }

}
