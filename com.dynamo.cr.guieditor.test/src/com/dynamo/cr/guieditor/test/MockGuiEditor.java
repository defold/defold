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
import org.eclipse.jface.action.IAction;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.operations.LinearUndoViolationUserApprover;

import com.dynamo.cr.guieditor.GuiSelectionProvider;
import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.GuiScene;
import com.dynamo.gui.proto.Gui.SceneDesc;

public abstract class MockGuiEditor implements IGuiEditor, IOperationHistoryListener, IEditorPart {

    UndoContext undoContext;
    DefaultOperationHistory history;
    GuiScene scene;
    private GuiSelectionProvider selectionProvider;

    public MockGuiEditor() {
    }

    public void init() {
        scene = new GuiScene(this, SceneDesc.newBuilder().setScript("").build());

        undoContext = new UndoContext();
        history = new DefaultOperationHistory();
        history.addOperationHistoryListener(this);
        history.setLimit(undoContext, 100);

        IOperationApprover approver = new LinearUndoViolationUserApprover(undoContext, this);
        history.addOperationApprover(approver);

        selectionProvider = new GuiSelectionProvider();
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
