package com.dynamo.cr.sceneed.core.test;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import javax.inject.Singleton;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Vector4d;

import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.swt.events.MouseEvent;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.editor.core.inject.LifecycleModule;
import com.dynamo.cr.editor.ui.IImageProvider;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.IClipboard;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.IManipulatorMode;
import com.dynamo.cr.sceneed.core.IManipulatorRegistry;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.ManipulatorController;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.TransformNodeOperation;
import com.dynamo.cr.sceneed.ui.RootManipulator;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class ManipulatorTest {

    private IManipulatorRegistry manipulatorRegistry;
    private IRenderView renderView;
    private ManipulatorController manipulatorController;
    private LifecycleModule module;
    private IOperationHistory undoHistory;
    private UndoContext undoContext;

    class TestModule extends AbstractModule {
        @Override
        protected void configure() {
            bind(IRenderView.class).toInstance(renderView);
            bind(IManipulatorRegistry.class).toInstance(manipulatorRegistry);
            bind(IOperationHistory.class).toInstance(undoHistory);
            bind(IUndoContext.class).toInstance(undoContext);
            bind(ManipulatorController.class).in(Singleton.class);
            bind(IClipboard.class).to(DummyClipboard.class).in(Singleton.class);

            // Heavy mocking of interfaces
            bind(INodeTypeRegistry.class).toInstance(mock(INodeTypeRegistry.class));
            bind(ISceneView.class).toInstance(mock(ISceneView.class));
            bind(ILoaderContext.class).toInstance(mock(ILoaderContext.class));
            bind(IContainer.class).toInstance(mock(IContainer.class));
            bind(IImageProvider.class).toInstance(mock(IImageProvider.class));
        }
    }

    @Before
    public void setup() throws CoreException, IOException {
        renderView = mock(IRenderView.class);
        undoContext = new UndoContext();
        undoHistory = new DefaultOperationHistory();

        manipulatorRegistry = Activator.getDefault().getManipulatorRegistry();
        module = new LifecycleModule(new TestModule());
        Injector injector = Guice.createInjector(module);

        manipulatorController = injector.getInstance(ManipulatorController.class);
    }

    @After
    public void shutdown() {
        module.close();
    }

    @Test
    public void testManipulatorSelection1() throws Exception {
        IManipulatorMode scaleMode = manipulatorRegistry.getMode(Activator.SCALE_MODE_ID);
        assertNotNull(scaleMode);
        List<Node> selection = new ArrayList<Node>();
        selection.add(new DummySphere());
        Manipulator manipulator = manipulatorRegistry.getManipulatorForSelection(scaleMode, selection.toArray(new Object[selection.size()]));
        assertNotNull(manipulator);
        assertThat(manipulator, instanceOf(DummySphereScaleManipulator.class));
    }

    @Test
    public void testManipulatorSelection2() throws Exception {
        IManipulatorMode scaleMode = manipulatorRegistry.getMode(Activator.SCALE_MODE_ID);
        assertNotNull(scaleMode);
        List<Node> selection = new ArrayList<Node>();
        selection.add(new DummyBox());
        Manipulator manipulator = manipulatorRegistry.getManipulatorForSelection(scaleMode, selection.toArray(new Object[selection.size()]));
        assertNull(manipulator);
    }

    void moveTo(Node node, Vector4d position) {
        Matrix4d originalTransform = new Matrix4d();
        node.getWorldTransform(originalTransform);

        Matrix4d newTransform = new Matrix4d();
        node.setTranslation(new Point3d(position.getX(), position.getY(), position.getZ()));
        node.getWorldTransform(newTransform);

        TransformNodeOperation operation = new TransformNodeOperation("Move", Arrays.asList(node), Arrays.asList(originalTransform), Arrays.asList(newTransform));
        manipulatorController.executeOperation(operation);
    }

    @Test
    public void testManipulatorRefresh() throws Exception {
        IManipulatorMode moveMode = manipulatorRegistry.getMode(Activator.MOVE_MODE_ID);
        manipulatorController.setManipulatorMode(moveMode);

        ArrayList<Node> selectionList = new ArrayList<Node>();
        DummySphere sphere = new DummySphere();
        selectionList.add(sphere);
        StructuredSelection selection = new StructuredSelection(selectionList);
        manipulatorController.setSelection(selection);
        manipulatorController.refresh();

        RootManipulator rootManipulator = manipulatorController.getRootManipulator();
        assertNotNull(rootManipulator);

        // Precondition
        assertThat(sphere.getTranslation().getX(), is(rootManipulator.getTranslation().getX()));

        // Change node position
        moveTo(sphere, new Vector4d(10, 0, 0, 0));

        manipulatorController.refresh();

        // Ensure that the manipulator follows..
        assertThat(sphere.getTranslation().getX(), is(rootManipulator.getTranslation().getX()));
    }

    @Test
    public void testMoveManipulator() throws Exception {
        IManipulatorMode moveMode = manipulatorRegistry.getMode(Activator.MOVE_MODE_ID);
        manipulatorController.setManipulatorMode(moveMode);

        ArrayList<Node> selectionList = new ArrayList<Node>();
        selectionList.add(new DummySphere());
        StructuredSelection selection = new StructuredSelection(selectionList);
        manipulatorController.setSelection(selection);

        RootManipulator rootManipulator = manipulatorController.getRootManipulator();
        assertNotNull(rootManipulator);
        Manipulator xAxis = (Manipulator) rootManipulator.getChildren().get(0);

        MouseEvent e = mock(MouseEvent.class);

        manipulatorController.initControl(Arrays.asList((Node) xAxis));

        assertThat(0, is(undoHistory.getUndoHistory(undoContext).length));
        manipulatorController.mouseDown(e);
        manipulatorController.mouseMove(e);
        manipulatorController.mouseUp(e);
        // Verify that a operation was executed
        assertThat(1, is(undoHistory.getUndoHistory(undoContext).length));
    }

    @Test
    public void testMoveManipulatorNop() throws Exception {
        IManipulatorMode moveMode = manipulatorRegistry.getMode(Activator.MOVE_MODE_ID);
        manipulatorController.setManipulatorMode(moveMode);

        ArrayList<Node> selectionList = new ArrayList<Node>();
        selectionList.add(new DummySphere());
        StructuredSelection selection = new StructuredSelection(selectionList);
        manipulatorController.setSelection(selection);

        MouseEvent e = mock(MouseEvent.class);
        assertThat(0, is(undoHistory.getUndoHistory(undoContext).length));
        manipulatorController.mouseUp(e);
        // Verify that *no* operation was executed
        assertThat(0, is(undoHistory.getUndoHistory(undoContext).length));
    }
}

