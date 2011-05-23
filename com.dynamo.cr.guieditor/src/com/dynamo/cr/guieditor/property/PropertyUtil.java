package com.dynamo.cr.guieditor.property;

import java.lang.reflect.Field;

public class PropertyUtil {

    @SuppressWarnings("null")
    public static Field findField(Class<?> klass, String name) throws SecurityException, NoSuchFieldException {
        while (klass != null) {
            Field[] fields = klass.getDeclaredFields();
            for (Field field : fields) {
                if (field.getName().equals(name)) {
                    return field;
                }
            }
            klass = klass.getSuperclass();
        }
        // Invoke getField in order to throw default NoSuchFieldException
        return klass.getField(name);
    }

}
