package com.dynamo.cr.sceneed.core.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

import javax.inject.Singleton;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Widget;
import org.junit.After;
import org.junit.Before;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

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
import com.dynamo.cr.sceneed.ui.RootManipulator;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public abstract class AbstractManipulatorTest {

    private static Widget dummyWidget = mock(Widget.class);

    private IManipulatorRegistry manipulatorRegistry;
    private IRenderView renderView;
    private ManipulatorController manipulatorController;
    private LifecycleModule module;
    private IOperationHistory undoHistory;
    private UndoContext undoContext;
    private int commandCount = 0;

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
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                int x = (Integer)invocation.getArguments()[0];
                int y = (Integer)invocation.getArguments()[1];
                Vector4d clickPos = (Vector4d)invocation.getArguments()[2];
                Vector4d clickDir = (Vector4d)invocation.getArguments()[3];
                clickPos.setX(x);
                clickPos.setY(y);
                clickDir.set(0, 0, -1, 0);
                return null;
            }
        }).when(this.renderView).viewToWorld(Mockito.anyInt(), Mockito.anyInt(), any(Vector4d.class), any(Vector4d.class));
        undoContext = new UndoContext();
        undoHistory = new DefaultOperationHistory();

        manipulatorRegistry = Activator.getDefault().getManipulatorRegistry();
        module = new LifecycleModule(new TestModule());
        Injector injector = Guice.createInjector(module);

        manipulatorController = injector.getInstance(ManipulatorController.class);

        commandCount = 0;
    }

    @After
    public void shutdown() {
        module.close();
    }

    protected static MouseEvent mouseDown() {
        Event e = new Event();
        e.button = SWT.BUTTON1;
        e.count = 1;
        e.widget = dummyWidget;
        return new MouseEvent(e);
    }

    protected static MouseEvent mouseUp() {
        Event e = new Event();
        e.button = SWT.BUTTON1;
        e.count = 1;
        e.widget = dummyWidget;
        return new MouseEvent(e);
    }

    protected static MouseEvent mouseMove(int x, int y) {
        Event e = new Event();
        e.x = x;
        e.y = y;
        e.count = 1;
        e.stateMask = SWT.BUTTON1;
        e.widget = dummyWidget;
        return new MouseEvent(e);
    }

    protected IManipulatorMode getManipulatorMode(String mode) {
        return manipulatorRegistry.getMode(mode);
    }

    protected void setManipulatorMode(String mode) {
        IManipulatorMode moveMode = getManipulatorMode(mode);
        manipulatorController.setManipulatorMode(moveMode);
    }

    protected RootManipulator getManipulatorForSelection(IManipulatorMode mode, Object[] selection) {
        return manipulatorRegistry.getManipulatorForSelection(mode, selection);
    }

    protected RootManipulator getRootManipulator() {
        return manipulatorController.getRootManipulator();
    }

    protected void refreshController() {
        manipulatorController.refresh();
    }

    protected List<Node> select(Node... nodes) {
        List<Node> selectionList = Arrays.asList(nodes);
        StructuredSelection selection = new StructuredSelection(selectionList);
        manipulatorController.setSelection(selection);
        return selectionList;
    }

    protected Manipulator handle(int axis) {
        RootManipulator rootManipulator = manipulatorController.getRootManipulator();
        return (Manipulator) rootManipulator.getChildren().get(axis);
    }

    protected void click(Manipulator m) {
        manipulatorController.initControl(Arrays.asList((Node) m));
        manipulatorController.mouseDown(mouseDown());
        manipulatorController.mouseUp(mouseUp());
    }

    protected void dragHold(Manipulator m, Vector3d screenDistance) {
        manipulatorController.initControl(Arrays.asList((Node) m));
        manipulatorController.mouseDown(mouseDown());
        manipulatorController.mouseMove(mouseMove((int)screenDistance.getX(), (int)screenDistance.getY()));
    }

    protected void dragRelease(Manipulator m, Vector3d screenDistance) {
        manipulatorController.initControl(Arrays.asList((Node) m));
        manipulatorController.mouseDown(mouseDown());
        manipulatorController.mouseMove(mouseMove((int)screenDistance.getX(), (int)screenDistance.getY()));
        manipulatorController.mouseUp(mouseUp());
    }

    protected void assertCommand() {
        assertThat(undoHistory.getUndoHistory(undoContext).length, is(++commandCount));
    }

    protected void assertNoCommand() {
        assertThat(undoHistory.getUndoHistory(undoContext).length, is(commandCount));
    }
}

