package com.dynamo.cr.properties.test.subpackage;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.properties.test.DummyClass;

public class SubDummyClass extends DummyClass {
    @Property
    @Range(min = 1, max = 10000)
    int subIntegerValue = 123;

    public int getSubIntegerValue() {
        return this.subIntegerValue;
    }

    public void setSubIntegerValue(int subIntegerValue) {
        this.subIntegerValue = subIntegerValue;
    }
}
