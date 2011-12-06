package com.dynamo.cr.sceneed.ui;

import org.eclipse.core.resources.IContainer;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;

import com.dynamo.cr.properties.FormPropertySheetPage;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.google.inject.Inject;

public class ScenePropertySheetPage extends FormPropertySheetPage {

    private final ISceneModel model;
    private final UndoActionHandler undoHandler;
    private final RedoActionHandler redoHandler;

    @Inject
    public ScenePropertySheetPage(IContainer contentRoot, ISceneModel model, UndoActionHandler undoHandler, RedoActionHandler redoHandler) {
        super(contentRoot);
        this.model = model;
        this.undoHandler = undoHandler;
        this.redoHandler = redoHandler;
    }

    @Override
    public void setActionBars(IActionBars actionBars) {
        super.setActionBars(actionBars);
        actionBars.setGlobalActionHandler(ActionFactory.UNDO.getId(), this.undoHandler);
        actionBars.setGlobalActionHandler(ActionFactory.REDO.getId(), this.redoHandler);
    }

    @Override
    public void selectionChanged(IWorkbenchPart part,
            ISelection selection) {
        if (selection instanceof IStructuredSelection) {
            IStructuredSelection structuredSelection = (IStructuredSelection)selection;
            for (Object object : structuredSelection.toArray()) {
                if (!(object instanceof Node)) {
                    getViewer().setInput(new Object[] {this.model.getRoot()});
                    return;
                }
            }
        }
        super.selectionChanged(part, selection);
    }

}
