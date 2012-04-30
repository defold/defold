package com.dynamo.cr.properties;

import java.lang.annotation.Annotation;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.MultiStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.graphics.RGB;

import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.descriptors.BooleanPropertyDesc;
import com.dynamo.cr.properties.descriptors.DoublePropertyDesc;
import com.dynamo.cr.properties.descriptors.FloatPropertyDesc;
import com.dynamo.cr.properties.descriptors.IntegerPropertyDesc;
import com.dynamo.cr.properties.descriptors.Point3PropertyDesc;
import com.dynamo.cr.properties.descriptors.ProtoEnumDesc;
import com.dynamo.cr.properties.descriptors.Quat4PropertyDesc;
import com.dynamo.cr.properties.descriptors.RGBPropertyDesc;
import com.dynamo.cr.properties.descriptors.ResourcePropertyDesc;
import com.dynamo.cr.properties.descriptors.TextPropertyDesc;
import com.dynamo.cr.properties.descriptors.Vector3PropertyDesc;
import com.dynamo.cr.properties.descriptors.Vector4PropertyDesc;
import com.google.common.collect.ArrayListMultimap;
import com.google.common.collect.Multimap;
import com.google.protobuf.ProtocolMessageEnum;

public class PropertyIntrospector<T, U extends IPropertyObjectWorld> {

    private Class<?> klass;
    private IPropertyDesc<T, U>[] descriptors;
    private ICommandFactory<T, U> commandFactory;
    private Class<? extends IPropertyAccessor<T, U>> accessorClass;
    private final Multimap<String, Annotation> validators = ArrayListMultimap.create();
    private final Multimap<String, Method> methodValidators = ArrayListMultimap.create();
    private final Set<String> properties = new HashSet<String>();

    public PropertyIntrospector(Class<?> klass) {
        this.klass = klass;
        try {
            introspect();
        } catch (Exception e) {
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
                this.accessorClass = (Class<? extends IPropertyAccessor<T, U>>) entityAnnotation.accessor();
            }

            Field[] fields = klass.getDeclaredFields();
            for (Field field : fields) {
                Annotation[] annotations = field.getAnnotations();
                for (Annotation annotation : annotations) {
                    String propertyId = field.getName();
                    if (annotation.annotationType() == Property.class) {
                        field.setAccessible(true);
                        Property property = (Property) annotation;
                        properties.add(propertyId);

                        String propertyDisplayName = propertyId;
                        if (!property.displayName().equals("")) {
                            propertyDisplayName = property.displayName();
                        }

                        IPropertyDesc<T, U> descriptor;
                        if (field.getType() == String.class) {
                            if (property.editorType() == EditorType.RESOURCE)
                                descriptor = new ResourcePropertyDesc<T, U>(propertyId, propertyDisplayName, property.extensions());
                            else
                                descriptor = new TextPropertyDesc<T, U>(propertyId, propertyDisplayName, property.editorType());
                        } else if (field.getType() == Vector4d.class) {
                            descriptor = new Vector4PropertyDesc<T, U>(propertyId, propertyDisplayName);
                        } else if (field.getType() == Quat4d.class) {
                            descriptor = new Quat4PropertyDesc<T, U>(propertyId, propertyDisplayName);
                        } else if (field.getType() == Vector3d.class) {
                            descriptor = new Vector3PropertyDesc<T, U>(propertyId, propertyDisplayName);
                        } else if (field.getType() == Point3d.class) {
                            descriptor = new Point3PropertyDesc<T, U>(propertyId, propertyDisplayName);
                        } else if (field.getType() == RGB.class) {
                            descriptor = new RGBPropertyDesc<T, U>(propertyId, propertyDisplayName);
                        } else if (field.getType() == Double.TYPE) {
                            descriptor = new DoublePropertyDesc<T, U>(propertyId, propertyDisplayName, property.editorType());
                        } else if (field.getType() == Float.TYPE) {
                            descriptor = new FloatPropertyDesc<T, U>(propertyId, propertyDisplayName, property.editorType());
                        } else if (field.getType() == Integer.TYPE) {
                            descriptor = new IntegerPropertyDesc<T, U>(propertyId, propertyDisplayName, property.editorType());
                        } else if (field.getType() == Boolean.TYPE) {
                            descriptor = new BooleanPropertyDesc<T, U>(propertyId, propertyDisplayName);
                        } else if (ProtocolMessageEnum.class.isAssignableFrom(field.getType())) {
                            descriptor = new ProtoEnumDesc<T, U>((Class<? extends ProtocolMessageEnum>) field.getType(), propertyId, propertyDisplayName);
                        } else {
                            descriptor = new PropertyDesc<T, U>(propertyId, propertyDisplayName);
                        }

                        descriptors.add(descriptor);

                        try {
                            Method validatorMethod = klass.getDeclaredMethod(String.format("validate%c%s", Character.toUpperCase(propertyId.charAt(0)), propertyId.substring(1)));
                            validatorMethod.setAccessible(true);
                            methodValidators.put(propertyId, validatorMethod);
                        } catch (NoSuchMethodException e) {
                            // Pass
                        }

                    } else if (annotation.annotationType().isAnnotationPresent(Validator.class)) {
                        this.validators.put(propertyId, annotation);
                    }
                }
            }

            klass = klass.getSuperclass();
        }

        this.descriptors = descriptors.toArray(new IPropertyDesc[descriptors.size()]);
    }

    public Object getPropertyValue(T object, U world, Object id) {
        IPropertyAccessor<T, U> accessor;
        try {
            accessor = accessorClass.newInstance();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        Object value = accessor.getValue(object, (String) id, world);
        return value;
    }

    public IUndoableOperation setPropertyValue(T object, U world, Object id, Object value) {
        IPropertyAccessor<T, U> accessor;
        try {
            accessor = accessorClass.newInstance();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        //accessor = new PropertyAccessorValidator(accessor);
        Object oldValue = accessor.getValue(object, (String) id, world);
        IUndoableOperation operation = commandFactory.create(object, (String) id, accessor, oldValue, value, world);
        return operation;
    }

    public boolean isPropertyEditable(T object, U world, Object id) {
        IPropertyAccessor<T, U> accessor;
        try {
            accessor = accessorClass.newInstance();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        boolean editable = accessor.isEditable(object, (String) id, world);
        return editable;
    }

    public boolean isPropertyVisible(T object, U world, Object id) {
        IPropertyAccessor<T, U> accessor;
        try {
            accessor = accessorClass.newInstance();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        boolean editable = accessor.isVisible(object, (String) id, world);
        return editable;
    }

    public Object[] getPropertyOptions(T object, U world, Object id) {
        IPropertyAccessor<T, U> accessor;
        try {
            accessor = accessorClass.newInstance();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        Object[] options = accessor.getPropertyOptions(object, (String) id, world);
        return options;
    }

    private IStatus validate(T obj, IPropertyAccessor<T, U> accessor, String property, U world) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException, InstantiationException {

        Object actualValue = accessor.getValue(obj, property, world);
        Collection<Annotation> list = validators.get(property);
        ArrayList<IStatus> statusList = new ArrayList<IStatus>(list.size());
        for (Annotation annotation : list) {
            Validator validatorClass = annotation.annotationType().getAnnotation(Validator.class);
            @SuppressWarnings("unchecked")
            IValidator<Object, Annotation, U> validator = (IValidator<Object, Annotation, U>) validatorClass.validator().newInstance();
            IStatus status = validator.validate(annotation, obj, property, actualValue, world);
            if (!status.isOK()) {
                statusList.add(status);
            }
        }

        Collection<Method> methods = methodValidators.get(property);
        if (methods != null) {
            for (Method method : methods) {
                IStatus status = (IStatus) method.invoke(obj);
                if (!status.isOK()) {
                    statusList.add(status);
                }
            }
        }

        if (statusList.size() == 1) {
            return statusList.get(0);
        }
        else if (statusList.size() > 0) {
            IStatus[] statusArray = statusList.toArray(new IStatus[statusList.size()]);
            return new MultiStatus(statusArray[0].getPlugin(), 0, statusArray, "Multi status", null);
        } else {
            return Status.OK_STATUS;
        }
    }

    public IStatus getObjectStatus(T object, U world) {
        MultiStatus status = new MultiStatus("com.dynamo.cr.properties", IStatus.OK, null, null);
        for (String property : this.properties) {
            // Use merge to keep a flat structure
            status.merge(getPropertyStatus(object, world, property));
        }
        if (status.isOK()) {
            return Status.OK_STATUS;
        } else {
            return status;
        }
    }

    public IStatus getPropertyStatus(T object, U world, Object id) {
        try {
            IPropertyAccessor<T, U> accessor = accessorClass.newInstance();
            IStatus status = Status.OK_STATUS;
            if (isPropertyVisible(object, world, id)) {
                return validate(object, accessor, (String) id, world);
            }
            return status;

        } catch (IllegalArgumentException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
        } catch (IllegalAccessException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
        } catch (InvocationTargetException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
        } catch (InstantiationException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
        }
        return Status.OK_STATUS;
    }

    public ICommandFactory<T, U> getCommandFactory() {
        return commandFactory;
    }

}
