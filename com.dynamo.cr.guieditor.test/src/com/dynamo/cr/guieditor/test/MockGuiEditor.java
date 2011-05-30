package com.dynamo.cr.guieditor.test;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationApprover;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IProject;
import org.eclipse.jface.action.IAction;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.operations.LinearUndoViolationUserApprover;

import com.dynamo.cr.guieditor.GuiSelectionProvider;
import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.render.IGuiRenderer;
import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.GuiScene;
import com.dynamo.gui.proto.Gui.SceneDesc;

public abstract class MockGuiEditor implements IGuiEditor, IOperationHistoryListener, IEditorPart {

    UndoContext undoContext;
    DefaultOperationHistory history;
    GuiScene scene;
    private GuiSelectionProvider selectionProvider;
    private IGuiRenderer renderer;
    private IProject project;

    public MockGuiEditor() {
    }

    public void init(IProject project, IGuiRenderer renderer) {
        scene = new GuiScene(this, SceneDesc.newBuilder().setScript("").build());
        this.project = project;

        undoContext = new UndoContext();
        history = new DefaultOperationHistory();
        history.addOperationHistoryListener(this);
        history.setLimit(undoContext, 100);

        IOperationApprover approver = new LinearUndoViolationUserApprover(undoContext, this);
        history.addOperationApprover(approver);

        selectionProvider = new GuiSelectionProvider();
        this.renderer = renderer;
    }

    @Override
    public IGuiRenderer getRenderer() {
        return renderer;
    }

    @Override
    public IContainer getContentRoot() {
        return project;
    }

    @Override
    public void executeOperation(IUndoableOperation operation) {
        operation.addContext(undoContext);
        try {
            history.execute(operation, null, null);
        } catch (final ExecutionException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public GuiScene getScene() {
        return scene;
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
    }

    @Override
    public GuiSelectionProvider getSelectionProvider() {
        return selectionProvider;
    }

    @Override
    public List<GuiNode> getSelectedNodes() {
        return new ArrayList<GuiNode>(selectionProvider.getSelectionList());
    }

    public IAction getAction(String id) {
        return null;
    }

}
