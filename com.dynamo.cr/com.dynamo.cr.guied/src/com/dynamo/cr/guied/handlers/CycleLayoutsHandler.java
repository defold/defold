package com.dynamo.cr.guied.handlers;

import java.util.List;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.guied.GuiSceneEditor;
import com.dynamo.cr.guied.core.GuiNode;
import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.guied.core.LayoutNode;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenter;
import com.dynamo.cr.sceneed.core.operations.SelectOperation;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;

public class CycleLayoutsHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);

        if (!(editorPart instanceof GuiSceneEditor)) {
            return null;
        }
        GuiSceneEditor sceneEditor = (GuiSceneEditor) editorPart;
        IStructuredSelection currentSelection = sceneEditor.getPresenterContext().getSelection();
        GuiSceneNode scene  = (GuiSceneNode) sceneEditor.getModel().getRoot();
        String currentLayoutId = scene.getCurrentLayout().getId();
        List<Node> childList = scene.getLayoutsNode().getChildren();
        int childIndex = 0;
        for(Node node : childList) {
            childIndex++;
            LayoutNode layoutNode = (LayoutNode) node;
            if(layoutNode.getId().compareTo(currentLayoutId)==0)
            {
                if(childIndex == childList.size()) {
                    childIndex = 0;
                }
                layoutNode = (LayoutNode) childList.get(childIndex);
                IStructuredSelection selection = new StructuredSelection(layoutNode);
                IPresenter presenter = (IPresenter) sceneEditor.getScenePresenter();
                IPresenterContext context = sceneEditor.getPresenterContext();
                context.executeOperation(new SelectOperation(currentSelection, selection, context));
                if(!currentSelection.isEmpty()) {
                    if(currentSelection.getFirstElement() instanceof GuiNode) {
                        presenter.onRefreshSceneView();
                        context.executeOperation(new SelectOperation(selection, currentSelection, context));
                        presenter.onRefreshSceneView();
                    }
                }
                break;
            }
        }
        return null;
    }

}
