package com.dynamo.cr.sceneed.core.test;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeUtil;

public class DummyIdNode extends DummyNode {

    public static class DummyIdFetcher implements NodeUtil.IdFetcher {
        @Override
        public String getId(Node child) {
            return ((DummyIdNode)child).getId();
        }
    }

    @Property
    private String id;

    public DummyIdNode(String id) {
        this.id = id;
    }

    public String getId() {
        return this.id;
    }

    public void setId(String id) {
        this.id = id;
    }
}
