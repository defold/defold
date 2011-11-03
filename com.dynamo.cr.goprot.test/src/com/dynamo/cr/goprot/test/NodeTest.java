package com.dynamo.cr.goprot.test;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.dynamo.cr.goprot.core.INodeView;
import com.dynamo.cr.goprot.core.INodeView.Presenter;
import com.dynamo.cr.goprot.core.Node;
import com.dynamo.cr.goprot.core.NodeModel;
import com.dynamo.cr.goprot.core.NodePresenter;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Inject;
import com.google.inject.Injector;
import com.google.inject.Singleton;

public class NodeTest {
    @Inject private Presenter presenter;
    private NodeModel model;
    private Injector injector;
    private INodeView view;

    class TestModule extends AbstractModule {
        @Override
        protected void configure() {
            bind(Presenter.class).to(NodePresenter.class);
            bind(NodeModel.class).in(Singleton.class);
            bind(INodeView.class).toInstance(view);
        }
    }

    @Before
    public void setup() throws CoreException, IOException {
        System.setProperty("java.awt.headless", "true");

        this.view = mock(INodeView.class);

        TestModule module = new TestModule();
        this.injector = Guice.createInjector(module);
        this.presenter = this.injector.getInstance(INodeView.Presenter.class);
        this.model = this.injector.getInstance(NodeModel.class);
        this.model.setRoot(new Node());
    }

    @Test
    public void testSelect() {
        Node node = this.model.getRoot();
        this.presenter.onSelect(node);
        assertTrue(node.isSelected());
        verify(this.view, times(1)).updateNode(node);
    }

}
