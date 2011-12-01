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

import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.MultiStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;
import org.eclipse.swt.graphics.RGB;

import com.dynamo.cr.properties.descriptors.BooleanPropertyDesc;
import com.dynamo.cr.properties.descriptors.DoublePropertyDesc;
import com.dynamo.cr.properties.descriptors.FloatPropertyDesc;
import com.dynamo.cr.properties.descriptors.IntegerPropertyDesc;
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
    private Multimap<String, Annotation> validators = ArrayListMultimap.create();
    private Multimap<String, Method> methodValidators = ArrayListMultimap.create();
    private Set<String> properties = new HashSet<String>();
    private Class<? extends NLS> nls;

    public PropertyIntrospector(Class<?> klass) {
        this.klass = klass;
        try {
            introspect();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    public PropertyIntrospector(Class<?> klass, Class<? extends NLS> nls) {
        this(klass);
        this.nls = nls;
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
                    if (annotation.annotationType() == Property.class) {
                        field.setAccessible(true);
                        Property property = (Property) annotation;
                        properties.add(field.getName());

                        IPropertyDesc<T, U> descriptor;
                        if (field.getType() == String.class) {
                            if (property.isResource())
                                descriptor = new ResourcePropertyDesc<T, U>(field.getName(), field.getName());
                            else
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
                        } else if (field.getType() == Float.TYPE) {
                            descriptor = new FloatPropertyDesc<T, U>(field.getName(), field.getName());
                        } else if (field.getType() == Integer.TYPE) {
                            descriptor = new IntegerPropertyDesc<T, U>(field.getName(), field.getName());
                        } else if (field.getType() == Boolean.TYPE) {
                            descriptor = new BooleanPropertyDesc<T, U>(field.getName(), field.getName());
                        } else if (ProtocolMessageEnum.class.isAssignableFrom(field.getType())) {
                            descriptor = new ProtoEnumDesc<T, U>((Class<? extends ProtocolMessageEnum>) field.getType(), field.getName(), field.getName());
                        } else {
                            descriptor = new PropertyDesc<T, U>(field.getName(), field.getName());
                        }

                        descriptors.add(descriptor);

                        try {
                            String name = field.getName();
                            Method validatorMethod = klass.getDeclaredMethod(String.format("validate%c%s", Character.toUpperCase(name.charAt(0)), name.substring(1)));
                            validatorMethod.setAccessible(true);
                            methodValidators.put(field.getName(), validatorMethod);
                        } catch (NoSuchMethodException e) {
                            // Pass
                        }

                    } else if (annotation.annotationType().isAnnotationPresent(Validator.class)) {
                        this.validators.put(field.getName(), annotation);
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

    private IStatus validate(T obj, IPropertyAccessor<T, U> accessor, String property, U world) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException, InstantiationException {

        Object actualValue = accessor.getValue(obj, property, world);
        Collection<Annotation> list = validators.get(property);
        ArrayList<IStatus> statusList = new ArrayList<IStatus>(list.size());
        for (Annotation annotation : list) {
            Validator validatorClass = annotation.annotationType().getAnnotation(Validator.class);
            @SuppressWarnings("unchecked")
            IValidator<Object, Annotation, U> validator = (IValidator<Object, Annotation, U>) validatorClass.validator().newInstance();
            IStatus status = validator.validate(annotation, property, actualValue, world, nls);
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
            status.add(getPropertyStatus(object, world, property));
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
            IStatus status = validate(object, accessor, (String) id, world);
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

    public boolean isValid(T object, U world) {
        try {
            IPropertyAccessor<T, U> accessor = accessorClass.newInstance();
            for (String property : properties) {
                if (validate(object, accessor, property, world).getSeverity() > IStatus.INFO) {
                    return false;
                }
            }
            return true;
        } catch (IllegalArgumentException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
            return true;
        } catch (IllegalAccessException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
            return true;
        } catch (InvocationTargetException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
            return true;
        } catch (InstantiationException e) {
            // Can't use any UI here in a core plugin such as StatusManager
            e.printStackTrace();
            return true;
        }

    }

    public ICommandFactory<T, U> getCommandFactory() {
        return commandFactory;
    }

}
