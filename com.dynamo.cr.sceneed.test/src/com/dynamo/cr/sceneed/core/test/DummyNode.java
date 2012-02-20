package com.dynamo.cr.sceneed.core.test;

import org.eclipse.core.resources.IFile;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Node;

public class DummyNode extends Node {

    @Property
    private int dummyProperty;

    public int getDummyProperty() {
        return this.dummyProperty;
    }

    public void setDummyProperty(int dummyProperty) {
        this.dummyProperty = dummyProperty;
    }

    public void addChild(DummyChild child) {
        super.addChild(child);
    }

    @Override
    public boolean handleReload(IFile file) {
        return file.exists();
    }
}
