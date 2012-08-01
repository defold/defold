package com.dynamo.cr.editor.ui.tip;

import org.eclipse.core.expressions.PropertyTester;

import com.dynamo.cr.editor.ui.EditorUIPlugin;

public class TipAvailablePropertyTester extends PropertyTester {

    public TipAvailablePropertyTester() {
    }

    @Override
    public boolean test(Object receiver, String property, Object[] args,
            Object expectedValue) {
        if (property.equals("tip_available")) {
            boolean expected = true;
            if (expectedValue != null) {
                expected = Boolean.parseBoolean((String) expectedValue);
            }
            ITipManager tipManager = EditorUIPlugin.getDefault().getInjector().getInstance(ITipManager.class);
            return tipManager.tipAvailable() == expected;
        }
        return false;
    }
}
