package com.dynamo.cr.sceneed.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.action.IStatusLineManager;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.sceneed.core.ISceneEditor;

public class CutNodesHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof ISceneEditor) {
            ISceneEditor editor = (ISceneEditor) editorPart;
            IStatusLineManager lineManager = editorPart.getEditorSite().getActionBars().getStatusLineManager();
            IProgressMonitor pm = lineManager.getProgressMonitor();
            try {
                editor.getScenePresenter().onCutSelection(editor.getPresenterContext(), editor.getLoaderContext(), pm);
            } catch (Exception e) {
                throw new ExecutionException(e.getMessage(), e);
            }
        }
        return null;
    }
}
