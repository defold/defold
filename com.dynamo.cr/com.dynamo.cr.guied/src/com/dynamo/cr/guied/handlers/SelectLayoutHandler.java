package com.dynamo.cr.guied.handlers;

import java.util.Map;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.commands.IElementUpdater;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.handlers.RadioState;
import org.eclipse.ui.menus.UIElement;

import com.dynamo.cr.guied.GuiSceneEditor;
import com.dynamo.cr.guied.core.GuiNode;
import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.guied.core.GuiScenePresenter;
import com.dynamo.cr.sceneed.core.ISceneEditor;

public class SelectLayoutHandler extends AbstractHandler implements IElementUpdater {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        if(HandlerUtil.matchesRadioState(event)) {
            return null;
        }
        String currentState = event.getParameter(RadioState.PARAMETER_ID);
        HandlerUtil.updateRadioState(event.getCommand(), currentState);
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof ISceneEditor) {
            ISceneEditor sceneEditor = (ISceneEditor) editorPart;
            IStructuredSelection currentSelection = sceneEditor.getPresenterContext().getSelection();

            GuiScenePresenter presenter = (GuiScenePresenter) sceneEditor.getNodePresenter(GuiSceneNode.class);
            presenter.onSelectLayoutNode(sceneEditor.getPresenterContext(), currentState);

            if(!currentSelection.isEmpty()) {
                if(currentSelection.getFirstElement() instanceof GuiNode) {
                    sceneEditor.getScenePresenter().onSelect(sceneEditor.getPresenterContext(), currentSelection);
                }
            }
        }
        return null;
    }

    @Override
    public void updateElement(UIElement element, @SuppressWarnings("rawtypes") Map parameters) {
        final IEditorPart editorPart = PlatformUI.getWorkbench().getActiveWorkbenchWindow().getActivePage().getActiveEditor();
        if (!(editorPart instanceof GuiSceneEditor)) {
            return;
        }
        GuiSceneNode scene  = (GuiSceneNode)((GuiSceneEditor)editorPart).getModel().getRoot();
        assert((scene instanceof GuiSceneNode) == true);
        String Id = (String) parameters.get("org.eclipse.ui.commands.radioStateParameter");
        element.setChecked(Id.compareTo(scene.getCurrentLayout().getId())==0 ? true : false);
    }

}
