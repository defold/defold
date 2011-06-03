package com.dynamo.cr.properties;

import java.lang.annotation.Annotation;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IResource;
import org.eclipse.jface.viewers.CellEditor;
import org.eclipse.jface.viewers.DialogCellEditor;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.ui.dialogs.ListDialog;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;
import org.eclipse.ui.views.properties.ColorPropertyDescriptor;
import org.eclipse.ui.views.properties.IPropertyDescriptor;
import org.eclipse.ui.views.properties.IPropertySource;
import org.eclipse.ui.views.properties.PropertyDescriptor;
import org.eclipse.ui.views.properties.TextPropertyDescriptor;

import com.dynamo.cr.properties.internal.DoublePropertyDescriptor;
import com.dynamo.cr.properties.internal.EmbeddedPropertySourceProxy;
import com.dynamo.cr.properties.internal.IntegerPropertyDescriptor;
import com.dynamo.cr.properties.internal.ProtoEnumDescriptor;
import com.google.protobuf.ProtocolMessageEnum;

public class PropertyIntrospectorSource<T, U extends IPropertyObjectWorld> implements IPropertySource {

    private T object;
    private U world;
    private IPropertyDescriptor[] descriptors;
    private Map<Object, Class<? extends IPropertyAccessor<T, U>>> idToAccessor = new HashMap<Object, Class<? extends IPropertyAccessor<T, U>>>();
    private Map<Object, ICommandFactory<T, U>> idToCommandFactory = new HashMap<Object, ICommandFactory<T, U>>();
    private Map<Object, IEmbeddedPropertySource<?>> idToEmeddedPropertySource = new HashMap<Object, IEmbeddedPropertySource<?>>();
    private IContainer contentRoot;

    public PropertyIntrospectorSource(T source, U world, IContainer contentRoot) {
        this.object = source;
        this.world = world;
        this.contentRoot = contentRoot;
        try {
            introspect();
        } catch (Throwable e) {
            throw new RuntimeException(e);
        }
    }

    class ResourcePropertyDescriptor extends PropertyDescriptor {

        public ResourcePropertyDescriptor(Object id, String displayName) {
            super(id, displayName);
        }

        @Override
        public CellEditor createPropertyEditor(Composite parent) {
            return new ResourceDialogCellEditor(parent);
        }
    }

    class ResourceDialogCellEditor extends DialogCellEditor {

        public ResourceDialogCellEditor(Composite parent) {
            super(parent, SWT.NONE);
        }

        @Override
        protected Object openDialogBox(Control cellEditorWindow) {

            ResourceListSelectionDialog dialog = new ResourceListSelectionDialog(getControl().getShell(), contentRoot, IResource.FILE | IResource.DEPTH_INFINITE);

            int ret = dialog.open();

            if (ret == ListDialog.OK)
            {
                IResource r = (IResource) dialog.getResult()[0];
                org.eclipse.core.runtime.IPath fullPath = r.getFullPath();
                return fullPath.makeRelativeTo(contentRoot.getFullPath()).toPortableString();
            }
            return null;
        }
    }

    @SuppressWarnings("unchecked")
    private void introspect() throws InstantiationException, IllegalAccessException, IllegalArgumentException, SecurityException, InvocationTargetException, NoSuchMethodException {
        List<IPropertyDescriptor> descriptors = new ArrayList<IPropertyDescriptor>();

        Class<?> klass = object.getClass();
        while (klass != null) {
            Field[] fields = klass.getDeclaredFields();
            for (Field field : fields) {
                Annotation[] annotations = field.getAnnotations();
                for (Annotation annotation : annotations) {
                    if (annotation.annotationType() == Property.class) {
                        field.setAccessible(true);
                        Property property = (Property) annotation;

                        IPropertyDescriptor descriptor;
                        if (field.getType() == String.class) {
                            if (property.isResource())
                                descriptor = new ResourcePropertyDescriptor(field.getName(), field.getName());
                             else
                                descriptor = new TextPropertyDescriptor(field.getName(), field.getName());
                        } else if (field.getType() == RGB.class) {
                            descriptor = new ColorPropertyDescriptor(field.getName(), field.getName());
                        } else if (field.getType() == Integer.TYPE) {
                            descriptor = new IntegerPropertyDescriptor(field.getName(), field.getName());
                        } else if (field.getType() == Double.TYPE) {
                            descriptor = new DoublePropertyDescriptor(field.getName(), field.getName());
                        } else if (ProtocolMessageEnum.class.isAssignableFrom(field.getType())) {
                            descriptor = new ProtoEnumDescriptor((Class<? extends ProtocolMessageEnum>) field.getType(), field.getName(), field.getName());
                        } else {
                            descriptor = new PropertyDescriptor(field.getName(), field.getName());
                        }

                        descriptors.add(descriptor);
                        idToAccessor.put(field.getName(), (Class<? extends IPropertyAccessor<T, U>>) property.accessor());
                        Class<? extends ICommandFactory<?, ?>> commandFactoryClass =  property.commandFactory();
                        ICommandFactory<T, U> commandFactory = (ICommandFactory<T, U>) commandFactoryClass.newInstance();
                        idToCommandFactory.put(field.getName(), commandFactory);

                        if (property.embeddedSource() != Property.DEFAULT_EMBEDDED_SOURCE.class) {
                            Class<? extends IEmbeddedPropertySource<?>> embeddedPropertySourceClass = property.embeddedSource();
                            idToEmeddedPropertySource.put(field.getName(), embeddedPropertySourceClass.newInstance());
                        }
                    }
                }
            }
            klass = klass.getSuperclass();
        }

        this.descriptors = descriptors.toArray(new IPropertyDescriptor[descriptors.size()]);
    }

    @Override
    public Object getEditableValue() {
        return object;
    }

    @Override
    public IPropertyDescriptor[] getPropertyDescriptors() {
        return descriptors;
    }

    @SuppressWarnings({ "rawtypes", "unchecked" })
    @Override
    public Object getPropertyValue(Object id) {
        Class<? extends IPropertyAccessor> accessorClass = idToAccessor.get(id);
        if (accessorClass == null) {
            return null;
        }

        try {
            IPropertyAccessor<T, U> accessor = accessorClass.newInstance();
            Object value = accessor.getValue(object, (String) id, world);

            IEmbeddedPropertySource<Object> embeddedPropertySource = (IEmbeddedPropertySource<Object>) idToEmeddedPropertySource.get(id);
            if (embeddedPropertySource != null) {
                ICommandFactory<T, U> commandFactory = idToCommandFactory.get(id);
                return new EmbeddedPropertySourceProxy(embeddedPropertySource, (Object) object, (String) id, accessor, commandFactory, world);
            }

            return value;

        } catch (IllegalArgumentException e) {
            e.printStackTrace();
            return null;
        } catch (IllegalAccessException e) {
            e.printStackTrace();
            return null;
        } catch (InvocationTargetException e) {
            e.printStackTrace();
            return null;
        } catch (InstantiationException e) {
            e.printStackTrace();
            return null;
        }
    }

    @Override
    public boolean isPropertySet(Object id) {
        return false;
    }

    @Override
    public void resetPropertyValue(Object id) {
    }

    @SuppressWarnings({ "rawtypes", "unchecked" })
    @Override
    public void setPropertyValue(Object id, Object value) {
        Class<? extends IPropertyAccessor> accessorClass = idToAccessor.get(id);
        try {
            IPropertyAccessor<T, U> accessor = accessorClass.newInstance();
            Object oldValue = accessor.getValue(object, (String) id, world);

            ICommandFactory<T, U> commandFactory = idToCommandFactory.get(id);
            commandFactory.createCommand(object, (String) id, accessor, oldValue, value, world);

        } catch (InstantiationException e) {
            e.printStackTrace();
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (IllegalArgumentException e) {
            e.printStackTrace();
        } catch (InvocationTargetException e) {
            e.printStackTrace();
        }
    }
}
