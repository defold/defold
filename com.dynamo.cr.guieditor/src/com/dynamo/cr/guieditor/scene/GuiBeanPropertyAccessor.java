package com.dynamo.cr.guieditor.scene;

import java.lang.reflect.InvocationTargetException;

import org.eclipse.core.runtime.IStatus;

import com.dynamo.cr.properties.BeanPropertyAccessor;
import com.dynamo.cr.properties.IPropertyObjectWorld;

public class GuiBeanPropertyAccessor extends BeanPropertyAccessor {

    @Override
    public IStatus getStatus(Object obj, String property,
            IPropertyObjectWorld world) throws IllegalArgumentException,
            IllegalAccessException, InvocationTargetException {

        if (obj instanceof GuiNode) {
            GuiNode guiNode = (GuiNode) obj;
            return guiNode.getStatus(property);
        }

        return super.getStatus(obj, property, world);
    }
}
