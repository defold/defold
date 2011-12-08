package com.dynamo.cr.sceneed.core.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import org.junit.Test;

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

        parent.addChild(new DummyIdNode("default1"));
        assertThat(NodeUtil.getUniqueId(parent, "default", idFetcher), is("default2"));
    }

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
}
