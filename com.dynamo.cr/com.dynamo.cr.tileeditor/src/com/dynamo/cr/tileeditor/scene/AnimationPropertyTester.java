package com.dynamo.cr.tileeditor.scene;

import org.eclipse.core.expressions.PropertyTester;

public class AnimationPropertyTester extends PropertyTester {

    public AnimationPropertyTester() {
        // TODO Auto-generated constructor stub
    }

    @Override
    public boolean test(Object receiver, String property, Object[] args,
            Object expectedValue) {
        if (receiver instanceof AnimationNode) {
            AnimationNode node = (AnimationNode)receiver;
            if (property.equals("playing")) {
                return new Boolean(node.isPlaying()).equals(expectedValue);
            }
        }
        return false;
    }

}
