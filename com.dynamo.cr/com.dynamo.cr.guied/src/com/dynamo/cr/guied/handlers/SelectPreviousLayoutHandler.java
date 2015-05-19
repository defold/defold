package com.dynamo.cr.guied.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.guied.GuiSceneEditor;
import com.dynamo.cr.guied.core.LayoutsNode;
import com.dynamo.cr.guied.core.GuiNode;
import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.sceneed.core.ISceneEditor;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenter;
import com.dynamo.cr.sceneed.core.Node;

public class SelectPreviousLayoutHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (!(editorPart instanceof GuiSceneEditor)) {
            return null;
        }
        if (editorPart instanceof ISceneEditor) {
            ISceneEditor sceneEditor = (ISceneEditor) editorPart;
            IStructuredSelection currentSelection = sceneEditor.getPresenterContext().getSelection();
            GuiSceneNode scene  = (GuiSceneNode) sceneEditor.getModel().getRoot();
            String prevLayout = scene.getPreviousLayout();
            if(prevLayout != null) {
                Node prevNode = ((LayoutsNode) scene.getLayoutsNode()).getLayoutNode(prevLayout);
                if(prevNode != null) {
                    IStructuredSelection selection = new StructuredSelection(prevNode);
                    IPresenter presenter = (IPresenter) sceneEditor.getScenePresenter();
                    presenter.onSelect(sceneEditor.getPresenterContext(), selection);
                    if(!currentSelection.isEmpty()) {
                        if(currentSelection.getFirstElement() instanceof GuiNode) {
                            presenter.onRefreshSceneView();
                            presenter.onSelect(sceneEditor.getPresenterContext(), currentSelection);
                            presenter.onRefreshSceneView();
                        }
                    }
                }
            }
        }
        return null;
    }

}
