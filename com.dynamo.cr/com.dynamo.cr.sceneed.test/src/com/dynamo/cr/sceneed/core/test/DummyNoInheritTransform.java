package com.dynamo.cr.sceneed.core.test;

import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class DummyNoInheritTransform extends Node {

    public DummyNoInheritTransform() {
        setFlags(Flags.NO_INHERIT_TRANSFORM);
    }

}
