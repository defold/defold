package com.dynamo.cr.properties;

import java.lang.reflect.Field;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;
import org.osgi.framework.Bundle;
import org.osgi.framework.FrameworkUtil;

public class ValidatorUtil {

    public static IStatus createStatus(Object object, String property, int severity, String suffix, String defaultMessage) {
        return createStatus(object, property, severity, suffix, defaultMessage, null);
    }

    public static IStatus createStatus(Object object, String property, int severity, String suffix, String defaultMessage, Object[] bindings) {
        String pluginId = "com.dynamo.cr.properties";
        String message = defaultMessage;
        Class<?> objectClass = object.getClass();
        Field field = null;
        while (field == null && objectClass != null) {
            try {
                field = objectClass.getDeclaredField(property);
            } catch (Exception e) {
                objectClass = objectClass.getSuperclass();
            }
        }
        if (field != null) {
            Package pkg = objectClass.getPackage();
            Bundle bundle = FrameworkUtil.getBundle(objectClass);
            pluginId = bundle.getSymbolicName();
            try {
                Class<?> msgClass = bundle.loadClass(String.format("%s.Messages", pkg.getName()));
                String msgKey = String.format("%s_%s_%s", objectClass.getSimpleName(), property, suffix);
                Field msgField = msgClass.getDeclaredField(msgKey);
                message = (String) msgField.get(null);
            } catch (Exception e) {
                // fall through
            }
        }
        return new Status(severity, pluginId, NLS.bind(message, bindings));
    }

}
