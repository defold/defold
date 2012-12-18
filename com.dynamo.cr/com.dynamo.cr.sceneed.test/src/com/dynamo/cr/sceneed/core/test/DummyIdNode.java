package com.dynamo.cr.sceneed.core.test;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Identifiable;

@SuppressWarnings("serial")
public class DummyIdNode extends DummyNode implements Identifiable {

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
