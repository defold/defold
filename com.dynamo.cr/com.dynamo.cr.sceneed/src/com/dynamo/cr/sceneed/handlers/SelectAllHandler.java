package com.dynamo.cr.sceneed.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.sceneed.core.ISceneEditor;
import org.eclipse.swt.widgets.*;

public class SelectAllHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {

        // This command handles selectAll event, which means they are eaten for any selected
        // text object, which is somewhat unintentional. So if a text control has focus, execute select all
        // there manually instead.
        Control focusControl = Display.getCurrent().getFocusControl();
        if (focusControl != null && focusControl instanceof Text) {
            Text textControl = (Text)focusControl;
            textControl.selectAll();
            return null;
        }

        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof ISceneEditor) {
            ISceneEditor editor = (ISceneEditor) editorPart;
            editor.getScenePresenter().onSelectAll(editor.getPresenterContext());
        }
        return null;
    }
}
