package com.dynamo.cr.properties;

import java.lang.reflect.Field;

import org.eclipse.osgi.util.NLS;

public class ValidateUtil {

    public static String bind(Class<? extends NLS> nls, String key, Object binding, String defaultMessage) {
        try {
            Field field = nls.getDeclaredField(key);
            String message = (String) field.get(null);
            return NLS.bind(message, binding);
        } catch (Throwable e) {
            return defaultMessage;
        }
    }

    public static String bind(Class<? extends NLS> nls, String key, String defaultMessage) {
        try {
            Field field = nls.getDeclaredField(key);
            String message = (String) field.get(null);
            return message;
        } catch (Throwable e) {
            return defaultMessage;
        }
    }

}
