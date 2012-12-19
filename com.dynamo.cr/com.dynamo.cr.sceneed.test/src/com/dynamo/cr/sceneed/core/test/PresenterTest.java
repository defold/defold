package com.dynamo.cr.sceneed.core.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyObject;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;

import java.io.ByteArrayInputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import javax.inject.Singleton;

import org.apache.commons.io.output.ByteArrayOutputStream;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.IClipboard;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.IModelListener;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.ScenePresenter;
import com.dynamo.cr.sceneed.ui.LoaderContext;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class PresenterTest extends AbstractPresenterTest {

    private ScenePresenter presenter;
    private ISceneView view;
    private IOperationHistory history;
    private IUndoContext undoContext;
    private INodeTypeRegistry nodeTypeRegistry;
    private IContainer contentRoot;

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(ISceneView.IPresenter.class).to(ScenePresenter.class).in(Singleton.class);
            bind(IModelListener.class).to(ScenePresenter.class).in(Singleton.class);
            bind(ISceneView.class).toInstance(view);
            bind(ISceneModel.class).toInstance(model);
            bind(INodeTypeRegistry.class).toInstance(nodeTypeRegistry);
            bind(ILoaderContext.class).to(LoaderContext.class).in(Singleton.class);
            bind(IClipboard.class).to(DummyClipboard.class).in(Singleton.class);

            bind(IOperationHistory.class).toInstance(history);
            bind(IUndoContext.class).toInstance(undoContext);

            bind(IContainer.class).toInstance(contentRoot);
        }
    }

    @SuppressWarnings("unchecked")
    @Override
    @Before
    public void setup() {
        super.setup();

        this.view = mock(ISceneView.class);
        this.contentRoot = mock(IContainer.class);
        this.nodeTypeRegistry = Activator.getDefault().getNodeTypeRegistry();

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
        doAnswer(new Answer<INodeType>() {
            @Override
            public INodeType answer(InvocationOnMock invocation) throws Throwable {
                Class<? extends Node> nodeClass = (Class<? extends Node>)invocation.getArguments()[0];
                return Activator.getDefault().getNodeTypeRegistry().getNodeTypeClass(nodeClass);
            }
        }).when(getPresenterContext()).getNodeType(any(Class.class));
        doAnswer(new Answer<INodeLoader<Node>>() {
            @Override
            public INodeLoader<Node> answer(InvocationOnMock invocation) throws Throwable {
                return Activator.getDefault().getNodeTypeRegistry().getNodeTypeClass((Class<Node>)invocation.getArguments()[0]).getLoader();
            }
        }).when(this.model).getNodeLoader((Class<Node>)anyObject());

        Injector injector = Guice.createInjector(new Module());

        this.presenter = (ScenePresenter)injector.getInstance(ISceneView.IPresenter.class);
        ILoaderContext loaderContext = injector.getInstance(ILoaderContext.class);
        setLoaderContext(loaderContext);
    }

    private void undo() throws ExecutionException {
        this.history.undo(this.undoContext, null, null);
    }

    private void redo() throws ExecutionException {
        this.history.redo(this.undoContext, null, null);
    }

    private List<Node> duplicate(List<Node> nodes) throws Exception {
        ByteArrayOutputStream byteOut = new ByteArrayOutputStream();
        ObjectOutputStream out = new ObjectOutputStream(byteOut);
        out.writeObject(nodes);
        byte[] data = byteOut.toByteArray();
        out.close();
        ObjectInputStream in = new ObjectInputStream(new ByteArrayInputStream(data));
        @SuppressWarnings("unchecked")
        List<Node> copies = (List<Node>)in.readObject();
        in.close();
        return copies;
    }

    @Test
    public void testSerial() throws Exception {
        DummyNode node = new DummyNode();
        node.setModel(this.model);
        DummyChild child = new DummyChild();
        child.setIntVal(1);
        node.addChild(child);
        DummyChild child2 = new DummyChild();
        child2.setIntVal(2);
        node.addChild(child2);

        List<Node> nodes = new ArrayList<Node>(2);
        nodes.add(child);
        nodes.add(child2);

        ByteArrayOutputStream outByte = new ByteArrayOutputStream();
        ObjectOutputStream out = new ObjectOutputStream(outByte);
        out.writeObject(nodes);
        byte[] data = outByte.toByteArray();
        ObjectInputStream in = new ObjectInputStream(new ByteArrayInputStream(data));
        @SuppressWarnings("unchecked")
        List<Node> nodes2 = (List<Node>)in.readObject();
        assertThat(nodes2.size(), is(2));
        DummyChild child3 = (DummyChild)nodes2.get(0);
        assertThat(child3.getIntVal(), is(1));
        DummyChild child4 = (DummyChild)nodes2.get(0);
        assertThat(child4.getIntVal(), is(1));
    }

    @Test
    public void testCopyPaste() throws Exception {
        DummyNode node = new DummyNode();
        node.setModel(this.model);
        DummyChild child = new DummyChild();
        child.setIntVal(1);
        DummyChild grandChild = new DummyChild();
        grandChild.setIntVal(3);
        child.addChild(grandChild);
        node.addChild(child);
        DummyChild child2 = new DummyChild();
        child2.setIntVal(2);
        node.addChild(child2);

        select(new Node[] {child, child2});

        this.presenter.onCopySelection(getPresenterContext(), getLoaderContext(), null);

        this.presenter.onPasteIntoSelection(getPresenterContext());

        assertThat(node.getChildren().size(), is(4));
        DummyChild child3 = (DummyChild)node.getChildren().get(2);
        assertTrue(child3.isVisible());
        assertThat(child3.getIntVal(), is(1));
        assertThat(child3.getModel(), is(this.model));
        List<Node> grandChildren = child3.getChildren();
        assertThat(grandChildren.size(), is(1));
        assertThat(((DummyChild)grandChildren.get(0)).getIntVal(), is(3));
        DummyChild child4 = (DummyChild)node.getChildren().get(3);
        assertThat(child4.getIntVal(), is(2));

        undo();
        assertThat(node.getChildren().size(), is(2));

        redo();
        assertThat(node.getChildren().size(), is(4));
    }

    @Test
    public void testCutPaste() throws Exception {
        DummyNode node = new DummyNode();
        node.setModel(this.model);
        DummyChild child = new DummyChild();
        child.setIntVal(1);
        node.addChild(child);
        DummyChild child2 = new DummyChild();
        child2.setIntVal(2);
        node.addChild(child2);

        select(new Node[] {child, child2});

        this.presenter.onCutSelection(getPresenterContext(), getLoaderContext(), null);

        assertThat(node.getChildren().size(), is(0));

        select(node);

        this.presenter.onPasteIntoSelection(getPresenterContext());

        assertThat(node.getChildren().size(), is(2));

        undo();
        assertThat(node.getChildren().size(), is(0));
        undo();
        assertThat(node.getChildren().size(), is(2));

        redo();
        assertThat(node.getChildren().size(), is(0));
        redo();
        assertThat(node.getChildren().size(), is(2));
    }

    @Test
    public void testDelete() throws Exception {
        DummyNode node = new DummyNode();
        node.setModel(this.model);
        DummyChild child = new DummyChild();
        child.setIntVal(1);
        node.addChild(child);
        DummyChild child2 = new DummyChild();
        child2.setIntVal(2);
        node.addChild(child2);

        select(new Node[] {child, child2});

        this.presenter.onDeleteSelection(getPresenterContext());

        assertThat(node.getChildren().size(), is(0));

        undo();
        assertThat(node.getChildren().size(), is(2));
        redo();
        assertThat(node.getChildren().size(), is(0));
    }

    private void dndMove(Node[] nodes, Node target) throws Exception {
        dndMove(nodes, target, target.getChildren().size());
    }

    private void dndMove(Node nodes[], Node target, int index) throws Exception {
        select(nodes);
        this.presenter.onDNDMoveSelection(getPresenterContext(), duplicate(Arrays.asList(nodes)), target, index);
    }

    private static int childCount(Node node) {
        return node.getChildren().size();
    }

    private static DummyChild getChild(Node node, int index) {
        return (DummyChild)node.getChildren().get(index);
    }

    @Test
    public void testDNDMoveExternal() throws Exception {
        DummyNode node = new DummyNode();
        node.setModel(this.model);
        DummyChild child = new DummyChild(node, 1);
        DummyChild child2 = new DummyChild(node, 2);

        DummyNode node2 = new DummyNode();

        dndMove(new Node[] {child, child2}, node2);

        assertThat(childCount(node), is(0));
        assertThat(childCount(node2), is(2));
        assertThat(getChild(node2, 0).getIntVal(), is(1));
        assertThat(getChild(node2, 1).getIntVal(), is(2));

        undo();
        assertThat(childCount(node2), is(0));
        assertThat(childCount(node), is(2));

        redo();
        assertThat(childCount(node), is(0));
        assertThat(childCount(node2), is(2));
    }

    private static void assertOrder(Node parent, int[] order) {
        int n = order.length;
        for (int i = 0; i < n; ++i) {
            assertThat(getChild(parent, i).getIntVal(), is(order[i]));
        }
    }

    @Test
    public void testDNDMoveInternal() throws Exception {
        DummyNode node = new DummyNode();
        node.setModel(this.model);
        DummyChild child[] = new DummyChild[5];
        for (int i = 0; i < 5; ++i) {
            child[i] = new DummyChild(node, i);
        }

        // Reorder to beginning
        dndMove(new Node[] {child[1], child[3]}, node, 0);
        assertOrder(node, new int[] {1, 3, 0, 2, 4});
        undo();
        assertOrder(node, new int[] {0, 1, 2, 3, 4});

        // Reorder to middle
        dndMove(new Node[] {child[1], child[3]}, node, 2);
        assertOrder(node, new int[] {0, 1, 3, 2, 4});
        undo();
        assertOrder(node, new int[] {0, 1, 2, 3, 4});

        // Reorder to end
        dndMove(new Node[] {child[1], child[3]}, node);
        assertOrder(node, new int[] {0, 2, 4, 1, 3});
        undo();
        assertOrder(node, new int[] {0, 1, 2, 3, 4});
    }

    private void dndDuplicate(Node[] nodes, Node target) throws Exception {
        dndDuplicate(nodes, target, target.getChildren().size());
    }

    private void dndDuplicate(Node nodes[], Node target, int index) throws Exception {
        select(nodes);
        this.presenter.onDNDDuplicateSelection(getPresenterContext(), duplicate(Arrays.asList(nodes)), target, index);
    }

    @Test
    public void testDNDDuplicateExternal() throws Exception {
        DummyNode node = new DummyNode();
        node.setModel(this.model);
        DummyChild child = new DummyChild(node, 1);
        DummyChild child2 = new DummyChild(node, 2);

        DummyNode node2 = new DummyNode();

        dndDuplicate(new Node[] {child, child2}, node2);

        assertThat(childCount(node), is(2));
        assertThat(childCount(node2), is(2));
        assertThat(getChild(node2, 0).getIntVal(), is(1));
        assertThat(getChild(node2, 1).getIntVal(), is(2));

        undo();
        assertThat(childCount(node2), is(0));
        assertThat(childCount(node), is(2));

        redo();
        assertThat(childCount(node), is(2));
        assertThat(childCount(node2), is(2));
    }

    @Test
    public void testDNDDuplicateInternal() throws Exception {
        DummyNode node = new DummyNode();
        node.setModel(this.model);
        DummyChild child[] = new DummyChild[5];
        for (int i = 0; i < 5; ++i) {
            child[i] = new DummyChild(node, i);
        }

        // Reorder to beginning
        dndDuplicate(new Node[] {child[1], child[3]}, node, 0);
        assertOrder(node, new int[] {1, 3, 0, 1, 2, 3, 4});
        undo();
        assertOrder(node, new int[] {0, 1, 2, 3, 4});

        // Reorder to middle
        dndDuplicate(new Node[] {child[1], child[3]}, node, 2);
        assertOrder(node, new int[] {0, 1, 1, 3, 2, 3, 4});
        undo();
        assertOrder(node, new int[] {0, 1, 2, 3, 4});

        // Reorder to end
        dndDuplicate(new Node[] {child[1], child[3]}, node);
        assertOrder(node, new int[] {0, 1, 2, 3, 4, 1, 3});
        undo();
        assertOrder(node, new int[] {0, 1, 2, 3, 4});
    }

}
