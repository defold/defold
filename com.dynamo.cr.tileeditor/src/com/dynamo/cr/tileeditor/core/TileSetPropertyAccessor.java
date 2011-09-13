package com.dynamo.cr.tileeditor.core;

import java.lang.reflect.InvocationTargetException;

import org.eclipse.core.runtime.IStatus;

import com.dynamo.cr.properties.BeanPropertyAccessor;
import com.dynamo.cr.properties.IPropertyObjectWorld;

public class TileSetPropertyAccessor extends BeanPropertyAccessor {
    @Override
    public IStatus getStatus(Object obj, String property,
            IPropertyObjectWorld world) throws IllegalArgumentException,
            IllegalAccessException, InvocationTargetException {
        if (world instanceof TileSetModel) {
            TileSetModel model = (TileSetModel)world;
            return model.getPropertyStatus(property);
        }
        return super.getStatus(obj, property, world);
    }
}
