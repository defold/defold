package com.dynamo.cr.tileeditor.handlers;

import java.util.Map;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.ISelectionService;
import org.eclipse.ui.IWorkbench;
import org.eclipse.ui.commands.IElementUpdater;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.handlers.RadioState;
import org.eclipse.ui.menus.UIElement;

import com.dynamo.cr.tileeditor.TileSetEditor2;
import com.dynamo.cr.tileeditor.scene.CollisionGroupNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;
import com.dynamo.cr.tileeditor.scene.TileSetNodePresenter;
import com.dynamo.cr.tileeditor.scene.TileSetUtil;

public class SelectCollisionGroupHandler extends AbstractHandler implements IElementUpdater {

    public static final String COMMAND_ID = "com.dynamo.cr.tileeditor.commands.selectCollisionGroup";

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart activeEditor = HandlerUtil.getActiveEditor(event);
        if (activeEditor instanceof TileSetEditor2) {
            if (HandlerUtil.matchesRadioState(event))
                return null; // we are already in the updated state - do nothing

            TileSetEditor2 editor = (TileSetEditor2)activeEditor;
            String currentState = event.getParameter(RadioState.PARAMETER_ID);
            int index = Integer.parseInt(currentState);
            TileSetNodePresenter presenter = (TileSetNodePresenter)editor.getNodePresenter(TileSetNode.class);
            presenter.onSelectCollisionGroup(editor.getPresenterContext(), index);

            // and finally update the current state
            HandlerUtil.updateRadioState(event.getCommand(), currentState);
        }
        return null;
    }

    /**
     * Needed to keep the menu state in sync with selection
     */
    @Override
    public void updateElement(UIElement element, @SuppressWarnings("rawtypes") Map parameters) {
        IWorkbench workbench = (IWorkbench) element.getServiceLocator().getService(IWorkbench.class);
        ISelectionService selectionService = null;
        if (workbench != null && workbench.getActiveWorkbenchWindow() != null) {
            selectionService = workbench.getActiveWorkbenchWindow().getSelectionService();
        }
        if (selectionService != null) {
            ISelection selection = selectionService.getSelection();
            if (selection instanceof IStructuredSelection) {
                IStructuredSelection structuredSelection = (IStructuredSelection)selection;
                int index = -1;
                CollisionGroupNode collisionGroup = TileSetUtil.getCurrentCollisionGroup(structuredSelection);
                if (collisionGroup != null) {
                    TileSetNode tileSet = collisionGroup.getTileSetNode();
                    index = tileSet.getCollisionGroups().indexOf(collisionGroup);
                }
                element.setChecked(parameters.get(RadioState.PARAMETER_ID).equals("" + index));
            }
        }
    }

}
