package com.dynamo.cr.sceneed.core.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.util.Collections;

import org.junit.Test;

import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeUtil;
import com.dynamo.cr.sceneed.core.test.DummyIdNode.DummyIdFetcher;

public class NodeUtilTest {

    @Test
    public void testUniqueId() {
        DummyNode parent = new DummyNode();
        parent.addChild(new DummyIdNode("default"));

        DummyIdFetcher idFetcher = new DummyIdFetcher();
        assertThat(NodeUtil.getUniqueId(parent, "default", idFetcher), is("default1"));
        assertThat(NodeUtil.getUniqueId(parent.getChildren(), "default", idFetcher), is("default1"));

        parent.addChild(new DummyIdNode("default1"));
        assertThat(NodeUtil.getUniqueId(parent, "default", idFetcher), is("default2"));
        assertThat(NodeUtil.getUniqueId(parent.getChildren(), "default", idFetcher), is("default2"));
    }

    @Test
    public void testSelectionReplacement() {
        DummyNode parent = new DummyNode();
        DummyNode child1 = new DummyNode();
        DummyNode child2 = new DummyNode();

        parent.addChild(child1);
        assertThat(NodeUtil.getSelectionReplacement(child1), is((Node)parent));

        parent.addChild(child2);
        assertThat(NodeUtil.getSelectionReplacement(child1), is((Node)child2));
        assertThat(NodeUtil.getSelectionReplacement(child2), is((Node)child1));
    }

    @Test
    public void testAcceptingParent() {
        // Mocking
        ISceneView.IPresenterContext presenterContext = mock(ISceneView.IPresenterContext.class);
        when(presenterContext.getNodeType(DummyNode.class)).thenReturn(Activator.getDefault().getNodeTypeRegistry().getNodeTypeClass(DummyNode.class));
        when(presenterContext.getNodeType(DummyChild.class)).thenReturn(Activator.getDefault().getNodeTypeRegistry().getNodeTypeClass(DummyChild.class));

        DummyNode node = new DummyNode();
        DummyChild child = new DummyChild();
        node.addChild(child);

        DummyNode node2 = new DummyNode();
        DummyChild child2 = new DummyChild();
        node2.addChild(child2);

        // Null
        Node parent = NodeUtil.findAcceptingParent(null, Collections.singletonList((Node)node), presenterContext);
        assertThat(parent, is((Node)null));

        // Empty source
        parent = NodeUtil.findAcceptingParent(node2, Collections.<Node>emptyList(), presenterContext);
        assertThat(parent, is((Node)node2));

        // Non-matching types
        parent = NodeUtil.findAcceptingParent(node2, Collections.singletonList((Node)node), presenterContext);
        assertThat(parent, is((Node)null));

        // Matching types
        parent = NodeUtil.findAcceptingParent(node2, Collections.singletonList((Node)child), presenterContext);
        assertThat(parent, is((Node)node2));

        // Descendants
        child.addChild(child2);
        parent = NodeUtil.findAcceptingParent(child2, Collections.singletonList((Node)child), presenterContext);
        assertThat(parent, is((Node)node));
        parent = NodeUtil.findAcceptingParent(child2, Collections.singletonList((Node)node), presenterContext);
        assertThat(parent, is((Node)null));

        // Unknown type
        @SuppressWarnings("serial")
        Node n = new Node() {};
        parent = NodeUtil.findAcceptingParent(n, Collections.singletonList((Node)node), presenterContext);
        assertThat(parent, is((Node)null));
        parent = NodeUtil.findAcceptingParent(node, Collections.singletonList(n), presenterContext);
        assertThat(parent, is((Node)null));
    }
}
