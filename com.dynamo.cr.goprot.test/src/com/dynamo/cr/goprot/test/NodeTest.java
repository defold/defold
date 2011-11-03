package com.dynamo.cr.goprot.test;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.dynamo.cr.goprot.core.INodeView.Presenter;
import com.dynamo.cr.goprot.core.Node;
import com.dynamo.cr.goprot.core.NodePresenter;
import com.google.inject.Module;

public class NodeTest extends AbstractTest {

    class TestModule extends GenericTestModule {
        @Override
        protected void configure() {
            super.configure();
            bind(Presenter.class).to(NodePresenter.class);
        }
    }

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();
        this.model.setRoot(new Node());
        this.manager.registerPresenter(Node.class, this.injector.getInstance(NodePresenter.class));
    }

    @Test
    public void testSelect() {
        Node node = this.model.getRoot();
        NodePresenter presenter = (NodePresenter)this.manager.getPresenter(node.getClass());
        presenter.onSelect(node);
        assertTrue(node.isSelected());
        verify(this.view, times(1)).updateNode(node);
    }

    @Override
    Module getModule() {
        return new TestModule();
    }

}
