package com.dynamo.cr.properties.test;

import com.dynamo.cr.properties.BeanPropertyAccessor;
import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Range;

@Entity(commandFactory = DummyCommandFactory.class, accessor = BeanPropertyAccessor.class)
public class TinyDummyClass2 {
    @Property
    @Range(min=1, max=10000)
    int integerValueA = 123;


    @Property
    @Range(min=0, max=100)
    int integerValueB = 1;

    @Property
    double doubleValue = 456;

    public int getIntegerValueA() {
        return this.integerValueA;
    }

    public void setIntegerValueA(int integerValue) {
        this.integerValueA = integerValue;
    }

    public int getIntegerValueB() {
        return this.integerValueB;
    }

    public void setIntegerValueB(int integerValue) {
        this.integerValueB = integerValue;
    }

    public double getDoubleValue() {
        return this.doubleValue;
    }

    public void setDoubleValue(double doubleValue) {
        this.doubleValue = doubleValue;
    }
}