package com.dynamo.cr.tileeditor.core;

import java.lang.reflect.InvocationTargetException;
import java.util.List;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.MultiStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.properties.BeanPropertyAccessor;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.tileeditor.Activator;

public class TileSetPropertyAccessor extends BeanPropertyAccessor {
    @Override
    public IStatus getStatus(Object obj, String property,
            IPropertyObjectWorld world) throws IllegalArgumentException,
            IllegalAccessException, InvocationTargetException {
        if (world instanceof TileSetModel) {
            TileSetModel model = (TileSetModel)world;
            List<Tag> tags = model.getPropertyTags(property);
            int n = tags.size();
            if (n > 0) {
                IStatus[] statuses = new IStatus[n];
                for (int i = 0; i < n; ++i) {
                    Tag tag = tags.get(i);
                    int severity = 0;
                    switch (tag.getType()) {
                    case Tag.TYPE_INFO:
                        severity = IStatus.INFO;
                        break;
                    case Tag.TYPE_WARNING:
                        severity = IStatus.WARNING;
                        break;
                    case Tag.TYPE_ERROR:
                        severity = IStatus.ERROR;
                        break;
                    }
                    statuses[i] = new Status(severity, Activator.PLUGIN_ID, tag.getMessage());
                }
                if (n > 1) {
                    return new MultiStatus(Activator.PLUGIN_ID, 0, statuses, "This property has multiple annotations", null);
                } else if (n > 0) {
                    return statuses[0];
                }
            }
        }
        return super.getStatus(obj, property, world);
    }
}
