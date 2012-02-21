package com.dynamo.cr.sceneed.core.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.UndoContext;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import com.dynamo.cr.editor.core.ILogger;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.ScenePresenter;
import com.dynamo.cr.sceneed.ui.LoaderContext;

public class PresenterTest extends AbstractPresenterTest {

    ScenePresenter presenter;
    ISceneView view;
    private IOperationHistory history;
    private IUndoContext undoContext;

    @Override
    @Before
    public void setup() {
        super.setup();

        this.view = mock(ISceneView.class);
        INodeTypeRegistry registry = Activator.getDefault().getNodeTypeRegistry();
        ILoaderContext loaderContext = new LoaderContext(null, Activator.getDefault().getNodeTypeRegistry(), null);
        this.presenter = new ScenePresenter(getModel(), this.view, registry, mock(ILogger.class), loaderContext, new TestClipboard(), null);
        this.history = new DefaultOperationHistory();
        this.undoContext = new UndoContext();
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                IUndoableOperation operation = (IUndoableOperation)invocation.getArguments()[0];
                operation.addContext(undoContext);
                history.execute(operation, null, null);
                return null;
            }
        }).when(getPresenterContext()).executeOperation(any(IUndoableOperation.class));
        setLoaderContext(loaderContext);
    }

    private void undo() throws ExecutionException {
        this.history.undo(this.undoContext, null, null);
    }

    private void redo() throws ExecutionException {
        this.history.redo(this.undoContext, null, null);
    }

    @Test
    public void testCopyPaste() throws Exception {
        DummyNode node = new DummyNode();
        EmbeddedChild child = new EmbeddedChild();
        child.setIntVal(1);
        child.setEmbedVal(1);
        node.addChild(child);
        EmbeddedChild child2 = new EmbeddedChild();
        child2.setIntVal(2);
        child2.setEmbedVal(2);
        node.addChild(child2);

        select(new Node[] {child, child2});

        this.presenter.onCopySelection(getPresenterContext(), getLoaderContext(), null);

        select(node);

        this.presenter.onPasteIntoSelection(getPresenterContext());

        assertThat(node.getChildren().size(), is(4));
        EmbeddedChild child3 = (EmbeddedChild)node.getChildren().get(2);
        assertThat(child3.getIntVal(), is(1));
        assertThat(child3.getEmbedVal(), is(1));
        EmbeddedChild child4 = (EmbeddedChild)node.getChildren().get(3);
        assertThat(child4.getIntVal(), is(2));
        assertThat(child4.getEmbedVal(), is(2));

        undo();
        assertThat(node.getChildren().size(), is(2));

        redo();
        assertThat(node.getChildren().size(), is(4));
    }

    @Test
    public void testCutPaste() throws Exception {
        DummyNode node = new DummyNode();
        EmbeddedChild child = new EmbeddedChild();
        child.setIntVal(1);
        child.setEmbedVal(1);
        node.addChild(child);
        EmbeddedChild child2 = new EmbeddedChild();
        child2.setIntVal(2);
        child2.setEmbedVal(2);
        node.addChild(child2);

        DummyNode node2 = new DummyNode();

        select(new Node[] {child, child2});

        this.presenter.onCutSelection(getPresenterContext(), getLoaderContext(), null);

        assertThat(node.getChildren().size(), is(0));

        select(node2);

        this.presenter.onPasteIntoSelection(getPresenterContext());

        assertThat(node2.getChildren().size(), is(2));
        EmbeddedChild child3 = (EmbeddedChild)node2.getChildren().get(0);
        assertThat(child3.getIntVal(), is(1));
        assertThat(child3.getEmbedVal(), is(1));
        EmbeddedChild child4 = (EmbeddedChild)node2.getChildren().get(1);
        assertThat(child4.getIntVal(), is(2));
        assertThat(child4.getEmbedVal(), is(2));

        undo();
        assertThat(node2.getChildren().size(), is(0));
        undo();
        assertThat(node.getChildren().size(), is(2));

        redo();
        assertThat(node.getChildren().size(), is(0));
        redo();
        assertThat(node2.getChildren().size(), is(2));
    }
}
