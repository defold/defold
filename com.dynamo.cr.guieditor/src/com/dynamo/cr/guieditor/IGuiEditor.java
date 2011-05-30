package com.dynamo.cr.guieditor;

import java.util.List;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IContainer;
import org.eclipse.jface.action.IAction;

import com.dynamo.cr.guieditor.render.IGuiRenderer;
import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.GuiScene;

public interface IGuiEditor {
    public void executeOperation(IUndoableOperation operation);

    public List<GuiNode> rectangleSelect(int x, int y, int w, int h);

    public GuiScene getScene();

    public List<GuiNode> getSelectedNodes();

    public GuiSelectionProvider getSelectionProvider();

    public IAction getAction(String id);

    public IGuiRenderer getRenderer();

    public IContainer getContentRoot();
}
