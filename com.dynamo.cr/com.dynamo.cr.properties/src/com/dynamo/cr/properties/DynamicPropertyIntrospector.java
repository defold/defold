package com.dynamo.cr.properties;

import java.lang.annotation.Annotation;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.MultiStatus;
import org.eclipse.core.runtime.Status;

public class DynamicPropertyIntrospector<T, U extends IPropertyObjectWorld> {
    static final String pluginId = "com.dynamo.cr.properties";

    private T object;
    private U world;
    private PropertyIntrospector<T, U> staticProperties;
    private IPropertyAccessor<T, U> accessor;
    private IValidator<Object, Annotation, U> validator;
    private Method descMethod;

    public DynamicPropertyIntrospector(T source, U world, PropertyIntrospector<T, U> staticProperties) {
        this.object = source;
        this.world = world;
        this.staticProperties = staticProperties;
        introspectDynamic();
    }

    private Method getMethod(Class<? extends Annotation> annotationClass, Method currentMethodValue, Method method) {
        Annotation a = method.getAnnotation(annotationClass);
        if (a != null) {
            if (currentMethodValue != null) {
                throw new RuntimeException(String.format("Multiple %s methods specified for %s", annotationClass.toString(), method.toString()));
            }
            return method;
        }

        return currentMethodValue;
    }

    @SuppressWarnings("unchecked")
    private void introspectDynamic() {
        Class<? extends Object> klass = object.getClass();
        Method[] methods = klass.getMethods();
        Method dynamicPropertyAccessorMethod = null;
        Method dynamicPropertyValidatorMethod = null;
        for (Method method : methods) {
            descMethod = getMethod(DynamicProperties.class, descMethod, method);
            dynamicPropertyAccessorMethod = getMethod(DynamicPropertyAccessor.class, dynamicPropertyAccessorMethod, method);
            dynamicPropertyValidatorMethod = getMethod(DynamicPropertyValidator.class, dynamicPropertyValidatorMethod, method);
        }

        if (dynamicPropertyAccessorMethod != null) {
            try {
                accessor = (IPropertyAccessor<T, U>) dynamicPropertyAccessorMethod.invoke(object, world);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        if (dynamicPropertyValidatorMethod != null) {
            try {
                validator = (IValidator<Object, Annotation, U>) dynamicPropertyValidatorMethod.invoke(object, world);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    @SuppressWarnings("unchecked")
    public IPropertyDesc<T, U>[] getDescriptors() {
        if (descMethod == null) {
            return (IPropertyDesc<T, U>[]) new IPropertyDesc[0];
        } else {
            try {
                IPropertyDesc<T, U>[] descriptions =  (IPropertyDesc<T, U>[]) descMethod.invoke(object);
                if (null != this.staticProperties) {
                    List<IPropertyDesc<T, U>> filtered = new ArrayList<IPropertyDesc<T, U>>(descriptions.length);
                    for (IPropertyDesc<T, U> desc : descriptions) {
                         if (!this.staticProperties.hasProperty(desc.getId())) {
                             filtered.add(desc);
                         }
                    }
                    descriptions = filtered.toArray(new IPropertyDesc[filtered.size()]);
                }
                return descriptions;
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    @SuppressWarnings("unchecked")
    private IPropertyDesc<T, U>[] getRejectedDescriptors() {
        if (descMethod == null) {
            return (IPropertyDesc<T, U>[]) new IPropertyDesc[0];
        } else {
            try {
                IPropertyDesc<T, U>[] descriptions =  (IPropertyDesc<T, U>[]) descMethod.invoke(object);
                if (null != this.staticProperties) {
                    List<IPropertyDesc<T, U>> filtered = new ArrayList<IPropertyDesc<T, U>>(descriptions.length);
                    for (IPropertyDesc<T, U> desc : descriptions) {
                         if (this.staticProperties.hasProperty(desc.getId())) {
                             filtered.add(desc);
                         }
                    }
                    descriptions = filtered.toArray(new IPropertyDesc[filtered.size()]);
                }
                return descriptions;
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    private Set<String> getProperties() {
        Set<String> props = new HashSet<String>();
        for (IPropertyDesc<T, U> pd : getDescriptors()) {
            props.add(pd.getId());
        }
        return props;
    }

    public IPropertyAccessor<T, U> getAccessor() {
        return this.accessor;
    }

    public IStatus getPropertyStatus(T object, U world, Object oid) {
        MultiStatus status = new MultiStatus(pluginId, IStatus.OK, null, null);
        String id = (String) oid;
        if (validator != null) {
            Object value = getAccessor().getValue(object, id, world);
            IStatus ps = validator.validate(null, object, id, value, world);
            if (!ps.isOK()) {
                status.merge(ps);
            }
        }
        for (IPropertyDesc<T, U> pd : getRejectedDescriptors()) {
            if (pd.getId().equals(id)) {
                Status reserved = new Status(IStatus.ERROR, pluginId, String.format(Messages.PropertyName_RESERVED, id));
                status.merge(reserved);
                break;
            }
        }
        return status;
    }

    public IStatus getObjectStatus(T object, U world) {
        MultiStatus status = new MultiStatus(pluginId, IStatus.OK, null, null);
        for (String property : getProperties()) {
            // Use merge to keep a flat structure
            IStatus ps = getPropertyStatus(object, world, property);
            if (!ps.isOK()) {
                status.merge(ps);
            }
        }
        for (IPropertyDesc<T, U> pd : getRejectedDescriptors()) {
            IStatus ps = getPropertyStatus(object, world, pd.getId());
            if (!ps.isOK()) {
                status.merge(ps);
            }
        }
        if (status.isOK()) {
            return Status.OK_STATUS;
        } else {
            return status;
        }
    }

    public Object[] getPropertyOptions(T object, U world, Object id) {
        Object[] options = accessor.getPropertyOptions(object, (String) id, world);
        return options;
    }
}

