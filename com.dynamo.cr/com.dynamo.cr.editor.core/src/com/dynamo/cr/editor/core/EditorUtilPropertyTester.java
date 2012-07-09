package com.dynamo.cr.editor.core;

import org.eclipse.core.expressions.PropertyTester;

public class EditorUtilPropertyTester extends PropertyTester {

    @Override
    public boolean test(Object receiver, String property, Object[] args, Object expectedValue) {
        if (property.equals("is_dev")) {
            boolean expected = true;
            if (expectedValue != null) {
                expected = Boolean.parseBoolean((String) expectedValue);
            }
            return EditorUtil.isDev() == expected;
        }
        return false;
    }

}
