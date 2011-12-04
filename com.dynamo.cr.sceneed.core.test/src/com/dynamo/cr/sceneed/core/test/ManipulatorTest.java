package com.dynamo.cr.sceneed.core.test;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.ui.ISelectionService;
import org.eclipse.ui.IWorkbenchPart;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.editor.core.ILogger;
import com.dynamo.cr.editor.core.inject.LifecycleModule;
import com.dynamo.cr.sceneed.core.Activator;
import com.dynamo.cr.sceneed.core.IManipulatorMode;
import com.dynamo.cr.sceneed.core.IManipulatorRegistry;
import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.ManipulatorController;
import com.dynamo.cr.sceneed.core.Node;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class ManipulatorTest {

    private IManipulatorRegistry manipulatorRegistry;
    private ISelectionService selectionService;
    private IRenderView renderView;
    private ManipulatorController manipulatorController;
    private LifecycleModule module;
    private IOperationHistory undoHistory;
    private UndoContext undoContext;
    private ILogger logger;

    class TestModule extends AbstractModule {
        @Override
        protected void configure() {
            bind(ISelectionService.class).toInstance(selectionService);
            bind(IRenderView.class).toInstance(renderView);
            bind(IManipulatorRegistry.class).toInstance(manipulatorRegistry);
            bind(ILogger.class).toInstance(logger);
            bind(IOperationHistory.class).toInstance(undoHistory);
            bind(IUndoContext.class).toInstance(undoContext);
        }
    }

    @Before
    public void setup() throws CoreException, IOException {
        selectionService = mock(ISelectionService.class);
        renderView = mock(IRenderView.class);
        undoContext = new UndoContext();
        undoHistory = new DefaultOperationHistory();
        logger = mock(ILogger.class);
        doThrow(new RuntimeException()).when(this.logger).logException(any(Throwable.class));

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
        IManipulatorMode sizeMode = manipulatorRegistry.getMode("com.dynamo.cr.sceneed.core.manipulators.size-mode");
        assertNotNull(sizeMode);
        List<Node> selection = new ArrayList<Node>();
        selection.add(new DummySphere());
        Manipulator manipulator = manipulatorRegistry.getManipulatorForSelection(sizeMode, selection.toArray(new Object[selection.size()]));
        assertNotNull(manipulator);
        assertThat(manipulator, instanceOf(TestSphereSizeManipulator.class));
    }

    @Test
    public void testManipulatorSelection2() throws Exception {
        IManipulatorMode sizeMode = manipulatorRegistry.getMode("com.dynamo.cr.sceneed.core.manipulators.size-mode");
        assertNotNull(sizeMode);
        List<Node> selection = new ArrayList<Node>();
        selection.add(new DummyBox());
        Manipulator manipulator = manipulatorRegistry.getManipulatorForSelection(sizeMode, selection.toArray(new Object[selection.size()]));
        assertNull(manipulator);
    }

    @Test
    public void testMoveManipulator() throws Exception {
        IManipulatorMode moveMode = manipulatorRegistry.getMode("com.dynamo.cr.sceneed.core.manipulators.move-mode");
        manipulatorController.setManipulatorMode(moveMode);

        IWorkbenchPart dummyPart = mock(IWorkbenchPart.class);
        ArrayList<Node> selectionList = new ArrayList<Node>();
        selectionList.add(new DummySphere());
        StructuredSelection selection = new StructuredSelection(selectionList);
        manipulatorController.selectionChanged(dummyPart, selection);

        MouseEvent e = mock(MouseEvent.class);
        assertThat(0, is(undoHistory.getUndoHistory(undoContext).length));
        manipulatorController.mouseUp(e);
        // Verify that the operation was executed
        assertThat(1, is(undoHistory.getUndoHistory(undoContext).length));
    }
}

