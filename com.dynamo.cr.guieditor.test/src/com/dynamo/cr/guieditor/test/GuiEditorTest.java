package com.dynamo.cr.guieditor.test;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;

import javax.vecmath.Vector4d;

import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.expressions.IEvaluationContext;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.ui.ISources;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mockito;

import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.commands.AddGuiBoxNode;
import com.dynamo.cr.guieditor.commands.AddGuiTextNode;
import com.dynamo.cr.guieditor.commands.Delete;
import com.dynamo.cr.guieditor.operations.DeleteGuiNodesOperation;
import com.dynamo.cr.guieditor.operations.SelectOperation;
import com.dynamo.cr.guieditor.operations.SetPropertiesOperation;
import com.dynamo.cr.guieditor.property.BeanPropertyAccessor;
import com.dynamo.cr.guieditor.property.IPropertyAccessor;
import com.dynamo.cr.guieditor.property.IPropertyObjectWorld;
import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.GuiScene;
import com.dynamo.cr.guieditor.scene.TextGuiNode;

public class GuiEditorTest {

    private IGuiEditor editor;
    private ExecutionEvent executionEvent;
    private DefaultOperationHistory history;
    private UndoContext undoContext;

    public static abstract class Context implements IEvaluationContext, ISelectionChangedListener {

        private ISelection selection;
        private IGuiEditor editor;

        public void init(IGuiEditor editor) {
            this.editor = editor;
            editor.getSelectionProvider().addSelectionChangedListener(this);
        }

        @Override
        public Object getVariable(String name) {
            if (name.equals(ISources.ACTIVE_EDITOR_NAME)) {
                return editor;
            } else if (name.equals(ISources.ACTIVE_CURRENT_SELECTION_NAME)) {
                return selection;
            }
            return null;
        }

        @Override
        public void selectionChanged(SelectionChangedEvent event) {
            selection = event.getSelection();
        }
    }

    @SuppressWarnings({ "rawtypes", "unchecked" })
    @Before
    public void setUp() throws Exception {
        MockGuiEditor mockEditor = mock(MockGuiEditor.class, Mockito.CALLS_REAL_METHODS);
        mockEditor.init();
        history = mockEditor.history;
        undoContext = mockEditor.undoContext;
        editor = mockEditor;
        HashMap params = new HashMap();
        params.put(ISources.ACTIVE_EDITOR_NAME, editor);

        Context context = mock(Context.class, Mockito.CALLS_REAL_METHODS);
        context.init(editor);

        executionEvent = new ExecutionEvent(null, params, null, context);
    }

    int nodeCount() {
        return editor.getScene().getNodeCount();
    }

    int selectCount() {
        return editor.getSelectedNodes().size();
    }

    @Test
    public void testAddGuiBoxNode() throws ExecutionException {
        AddGuiBoxNode command = new AddGuiBoxNode();
        command.execute(executionEvent);

        assertEquals(1, nodeCount());
        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(0, nodeCount());
    }

    @Test
    public void testAddGuiTextNode() throws ExecutionException {
        AddGuiTextNode command = new AddGuiTextNode();
        command.execute(executionEvent);

        assertEquals(1, nodeCount());
        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(0, nodeCount());
    }

    @Test
    public void testDeleteNodeOperation() throws ExecutionException {
        AddGuiBoxNode command = new AddGuiBoxNode();
        command.execute(executionEvent);

        assertEquals(1, nodeCount());
        GuiNode node = editor.getScene().getNode(0);

        DeleteGuiNodesOperation operation = new DeleteGuiNodesOperation(editor, Arrays.asList(node));
        editor.executeOperation(operation);
        assertEquals(0, nodeCount());

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(1, nodeCount());
    }

    @Test
    public void testSelectOperationDeleteCommand() throws ExecutionException {
        AddGuiBoxNode command = new AddGuiBoxNode();
        command.execute(executionEvent);

        assertEquals(1, nodeCount());
        GuiNode node = editor.getScene().getNode(0);

        SelectOperation operation = new SelectOperation(editor.getSelectionProvider(), new ArrayList<GuiNode>(), Arrays.asList(node));
        editor.executeOperation(operation);
        assertEquals(1, selectCount());

        Delete deleteCommand = new Delete();
        deleteCommand.execute(executionEvent);
        assertEquals(0, nodeCount());
        // TODO: Shoule be zero but mockeditor doesn't listen on scene events... Move selection functionality from GuiEditor to Scene?
        //assertEquals(0, selectCount());

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(1, nodeCount());
        assertEquals(1, selectCount());

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(0, selectCount());
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testSetPosition() throws ExecutionException {
        AddGuiBoxNode command = new AddGuiBoxNode();
        command.execute(executionEvent);

        assertEquals(1, nodeCount());
        GuiNode node = editor.getScene().getNode(0);
        node.setPosition(new Vector4d());

        IPropertyAccessor<?, ? extends IPropertyObjectWorld> tmp = new BeanPropertyAccessor();
        IPropertyAccessor<Object, GuiScene> accessor = (IPropertyAccessor<Object, GuiScene>) tmp;
        SetPropertiesOperation<Vector4d> operation = new SetPropertiesOperation<Vector4d>(node, "position", accessor, new Vector4d(), new Vector4d(1,2,3,0), editor.getScene());;
        editor.executeOperation(operation);
        assertEquals(node.getPosition(), new Vector4d(1,2,3,0));

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(node.getPosition(), new Vector4d(0,0,0,0));
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testSetText() throws ExecutionException {
        AddGuiTextNode command = new AddGuiTextNode();
        command.execute(executionEvent);

        assertEquals(1, nodeCount());
        TextGuiNode node = (TextGuiNode) editor.getScene().getNode(0);
        node.setPosition(new Vector4d());

        IPropertyAccessor<?, ? extends IPropertyObjectWorld> tmp = new BeanPropertyAccessor();
        IPropertyAccessor<Object, GuiScene> accessor = (IPropertyAccessor<Object, GuiScene>) tmp;
        SetPropertiesOperation<String> operation = new SetPropertiesOperation<String>(node, "text", accessor, "", "foo", editor.getScene());;
        editor.executeOperation(operation);
        assertEquals(node.getText(), "foo");

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(node.getText(), "");
    }

}
