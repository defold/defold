package com.dynamo.cr.guieditor.property;

import java.lang.reflect.Field;

import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.GuiScene;

public class FieldAccessor implements IPropertyAccessor<GuiNode, GuiScene> {

    private Field field;

    private void init(GuiNode obj, String property) {
        if (field != null)
            return;

        try {
            field = PropertyUtil.findField(obj.getClass(), property);
            field.setAccessible(true);
        } catch (Throwable e) {
            throw new IllegalArgumentException(e);
        }
    }

    protected Field getField(GuiNode obj, String property) {
        init(obj, property);
        return field;
    }

    public void setValue(GuiNode obj, String property, Object value, GuiScene world) throws IllegalArgumentException, IllegalAccessException {
        init(obj, property);
        field.set(obj, value);
    }

    @Override
    public Object getValue(GuiNode obj, String property, GuiScene world) throws IllegalArgumentException, IllegalAccessException {
        init(obj, property);
        return field.get(obj);
    }
}
