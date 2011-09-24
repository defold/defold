package com.dynamo.cr.goeditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.goeditor.Component;
import com.dynamo.cr.goeditor.GameObjectEditor2;

public class RemoveComponent extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        GameObjectEditor2 editor = (GameObjectEditor2) editorPart;

        // This command is only valid for selections of Components. Therefore we don't check here.
        // If the command is invoked with empty or selection of different type there is a bug in the enabledWhen declaration
        IStructuredSelection selection = (IStructuredSelection) HandlerUtil.getActiveWorkbenchWindow(event).getActivePage().getSelection();
        Component component = (Component) selection.getFirstElement();
        editor.getPresenter().onRemoveComponent(component);
        return null;
    }

}
