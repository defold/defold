package com.dynamo.cr.sceneed.core.test;

import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class DummyChild extends Node {

    private int intVal = 0;

    public DummyChild() {

    }

    public DummyChild(Node parent, int intVal) {
        this.intVal = intVal;
        parent.addChild(this);
    }

    public int getIntVal() {
        return this.intVal;
    }

    public void setIntVal(int intVal) {
        this.intVal = intVal;
    }

}
