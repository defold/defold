package com.dynamo.cr.properties;

import java.lang.annotation.Annotation;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.swt.graphics.RGB;

import com.dynamo.cr.properties.descriptors.DoublePropertyDesc;
import com.dynamo.cr.properties.descriptors.ProtoEnumDesc;
import com.dynamo.cr.properties.descriptors.Quat4PropertyDesc;
import com.dynamo.cr.properties.descriptors.RGBPropertyDesc;
import com.dynamo.cr.properties.descriptors.TextPropertyDesc;
import com.dynamo.cr.properties.descriptors.Vector3PropertyDesc;
import com.dynamo.cr.properties.descriptors.Vector4PropertyDesc;
import com.google.protobuf.ProtocolMessageEnum;

public class PropertyIntrospector<T, U extends IPropertyObjectWorld> {

    private Class<?> klass;
    private Map<Object, Class<? extends IPropertyAccessor<T, U>>> idToAccessor = new HashMap<Object, Class<? extends IPropertyAccessor<T, U>>>();
    private IPropertyDesc<T, U>[] descriptors;
    private ICommandFactory<T, U> commandFactory;

    public PropertyIntrospector(Class<?> klass) {
        this.klass = klass;
        try {
            introspect();
        } catch (Throwable e) {
            throw new RuntimeException(e);
        }
    }

    public IPropertyDesc<T, U>[] getDescriptors() {
        return descriptors;
    }

    @SuppressWarnings("unchecked")
    private void introspect() throws InstantiationException, IllegalAccessException, IllegalArgumentException, SecurityException, InvocationTargetException, NoSuchMethodException {
        List<IPropertyDesc<T, U>> descriptors = new ArrayList<IPropertyDesc<T, U>>();

        while (klass != null) {

            Entity entityAnnotation = klass.getAnnotation(Entity.class);
            if (entityAnnotation != null) {
                this.commandFactory = (ICommandFactory<T, U>) entityAnnotation.commandFactory().newInstance();
            }

            Field[] fields = klass.getDeclaredFields();
            for (Field field : fields) {
                Annotation[] annotations = field.getAnnotations();
                for (Annotation annotation : annotations) {
                    if (annotation.annotationType() == Property.class) {
                        field.setAccessible(true);
                        Property property = (Property) annotation;

                        IPropertyDesc<T, U> descriptor;
                        if (field.getType() == String.class) {
                            // TODO: Add support for resources
                            descriptor = new TextPropertyDesc<T, U>(field.getName(), field.getName());
                        } else if (field.getType() == Vector4d.class) {
                            descriptor = new Vector4PropertyDesc<T, U>(field.getName(), field.getName());
                        } else if (field.getType() == Quat4d.class) {
                            descriptor = new Quat4PropertyDesc<T, U>(field.getName(), field.getName());
                        } else if (field.getType() == Vector3d.class) {
                            descriptor = new Vector3PropertyDesc<T, U>(field.getName(), field.getName());
                        }
                        else if (field.getType() == RGB.class) {
                            descriptor = new RGBPropertyDesc<T, U>(field.getName(), field.getName());
                        } else if (field.getType() == Double.TYPE) {
                            descriptor = new DoublePropertyDesc<T, U>(field.getName(), field.getName());
                        } else if (ProtocolMessageEnum.class.isAssignableFrom(field.getType())) {
                            descriptor = new ProtoEnumDesc<T, U>((Class<? extends ProtocolMessageEnum>) field.getType(), field.getName(), field.getName());
                        } else {
                            descriptor = new PropertyDesc<T, U>(field.getName(), field.getName());
                        }

                        descriptors.add(descriptor);
                        idToAccessor.put(field.getName(), (Class<? extends IPropertyAccessor<T, U>>) property.accessor());
                    }
                }
            }
            klass = klass.getSuperclass();
        }

        this.descriptors = descriptors.toArray(new IPropertyDesc[descriptors.size()]);
    }

    @SuppressWarnings({ "rawtypes", "unchecked" })
    public Object getPropertyValue(T object, U world, Object id) {
        Class<? extends IPropertyAccessor> accessorClass = idToAccessor.get(id);
        if (accessorClass == null) {
            return null;
        }

        try {
            IPropertyAccessor<T, U> accessor = accessorClass.newInstance();
            Object value = accessor.getValue(object, (String) id, world);
            return value;

        } catch (IllegalArgumentException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
            return null;
        } catch (IllegalAccessException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
            return null;
        } catch (InvocationTargetException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
            return null;
        } catch (InstantiationException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
            return null;
        }
    }


    @SuppressWarnings({ "rawtypes", "unchecked" })
    public IUndoableOperation setPropertyValue(T object, U world, Object id, Object value) {
        Class<? extends IPropertyAccessor> accessorClass = idToAccessor.get(id);
        try {
            IPropertyAccessor<T, U> accessor = accessorClass.newInstance();
            Object oldValue = accessor.getValue(object, (String) id, world);
            IUndoableOperation operation = commandFactory.create(object, (String) id, accessor, oldValue, value, world);
            return operation;

        } catch (InstantiationException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
        } catch (IllegalAccessException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
        } catch (IllegalArgumentException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
        } catch (InvocationTargetException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
        }
        return null;
    }

    @SuppressWarnings({ "rawtypes", "unchecked" })
    public IStatus getPropertyStatus(T object, U world, Object id) {
        Class<? extends IPropertyAccessor> accessorClass = idToAccessor.get(id);
        if (accessorClass == null) {
            return null;
        }

        try {
            IPropertyAccessor<T, U> accessor = accessorClass.newInstance();
            IStatus status = accessor.getStatus(object, (String) id, world);
            return status;

        } catch (IllegalArgumentException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
            return null;
        } catch (IllegalAccessException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
            return null;
        } catch (InvocationTargetException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
            return null;
        } catch (InstantiationException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
            return null;
        }
    }

    public ICommandFactory<T, U> getCommandFactory() {
        return commandFactory;
    }

}
