package com.dynamo.cr.goeditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.goeditor.Component;
import com.dynamo.cr.goeditor.EmbeddedComponent;
import com.dynamo.cr.goeditor.GameObjectModel;
import com.dynamo.cr.goeditor.GameObjectPresenter;
import com.dynamo.cr.goeditor.IGameObjectView;
import com.dynamo.cr.goeditor.ILogger;
import com.dynamo.cr.goeditor.ResourceComponent;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.protobuf.DescriptorProtos;
import com.google.protobuf.Message;

public class GameObjectTest {

    private IGameObjectView view;
    private IGameObjectView.Presenter presenter;
    private GameObjectModel model;
    private DefaultOperationHistory history;
    private UndoContext undoContext;

    class TestLogger implements ILogger {

        @Override
        public void logException(Throwable exception) {
            exception.printStackTrace();
        }

    }

    class TestModule extends AbstractModule {
        @Override
        protected void configure() {
            bind(IGameObjectView.class).toInstance(view);
            bind(GameObjectModel.class).toInstance(model);

            bind(IOperationHistory.class).toInstance(history);
            bind(IUndoContext.class).toInstance(undoContext);

            bind(ILogger.class).toInstance(new TestLogger());
        }
    }

    @Before
    public void setup() {
        history = new DefaultOperationHistory();
        undoContext = new UndoContext();


        view = mock(IGameObjectView.class);
        model = new GameObjectModel();

        presenter = new GameObjectPresenter();

        TestModule module = new TestModule();
        Injector injector = Guice.createInjector(module);
        presenter = injector.getInstance(GameObjectPresenter.class);
    }


    // Helper functions
    void undo() throws ExecutionException {
        history.undo(undoContext, null, null);
    }

    void redo() throws ExecutionException {
        history.redo(undoContext, null, null);
    }

    Component component(int index) {
        return model.getComponents().get(index);
    }

    String resource(int index) {
        return ((ResourceComponent) component(index)).getResource();
    }

    Message message(int index) {
        return ((EmbeddedComponent) component(index)).getMessage();
    }

    // Actual tests
    @Test
    public void testAddResourceComponent() throws Exception {
        when(view.openAddResourceComponentDialog()).thenReturn("test.script");
        presenter.onAddResourceComponent();

        assertThat(model.getComponents().size(), is(1));
        assertThat(resource(0), is("test.script"));

        undo();
        assertThat(model.getComponents().size(), is(0));

        redo();
        assertThat(model.getComponents().size(), is(1));
        assertThat(resource(0), is("test.script"));
    }

    @Test
    public void testAddEmbeddedComponent() throws Exception {
        Message m1 = DescriptorProtos.FileDescriptorProto.getDefaultInstance();
        Message m2 = DescriptorProtos.ServiceDescriptorProto.getDefaultInstance();

        when(view.openAddEmbeddedComponentDialog()).thenReturn(m1, m2);
        presenter.onAddEmbeddedComponent();
        presenter.onAddEmbeddedComponent();

        assertThat(model.getComponents().size(), is(2));
        assertThat(message(0), is(m1));
        assertThat(message(1), is(m2));

        undo();
        assertThat(model.getComponents().size(), is(1));

        undo();
        assertThat(model.getComponents().size(), is(0));

        redo();
        redo();
        assertThat(model.getComponents().size(), is(2));
        assertThat(message(0), is(m1));
        assertThat(message(1), is(m2));
    }

    @Test
    public void testPreserveOrder() throws Exception {
        when(view.openAddResourceComponentDialog()).thenReturn("test1.script", "test2.script", "test3.script");
        presenter.onAddResourceComponent();
        presenter.onAddResourceComponent();
        presenter.onAddResourceComponent();

        assertThat(model.getComponents().size(), is(3));
        assertThat(resource(0), is("test1.script"));
        assertThat(resource(1), is("test2.script"));
        assertThat(resource(2), is("test3.script"));

        undo();
        undo();
        redo();
        redo();
        assertThat(model.getComponents().size(), is(3));
        assertThat(resource(0), is("test1.script"));
        assertThat(resource(1), is("test2.script"));
        assertThat(resource(2), is("test3.script"));

        presenter.onRemoveComponent(component(1));
        assertThat(model.getComponents().size(), is(2));
        assertThat(resource(0), is("test1.script"));
        assertThat(resource(1), is("test3.script"));

        undo();
        assertThat(model.getComponents().size(), is(3));
        assertThat(resource(0), is("test1.script"));
        assertThat(resource(1), is("test2.script"));
        assertThat(resource(2), is("test3.script"));
    }

}
