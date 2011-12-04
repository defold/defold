package com.dynamo.cr.properties.test;

import com.dynamo.cr.properties.BeanPropertyAccessor;
import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Range;

@Entity(commandFactory = DummyCommandFactory.class, accessor = BeanPropertyAccessor.class)
public class TinyDummyClass {
    @Property
    @Range(min=1, max=10000)
    int integerValue = 123;

    @Property
    double doubleValue = 456;

    public int getIntegerValue() {
        return this.integerValue;
    }

    public void setIntegerValue(int integerValue) {
        this.integerValue = integerValue;
    }

    public double getDoubleValue() {
        return this.doubleValue;
    }

    public void setDoubleValue(double doubleValue) {
        this.doubleValue = doubleValue;
    }
}