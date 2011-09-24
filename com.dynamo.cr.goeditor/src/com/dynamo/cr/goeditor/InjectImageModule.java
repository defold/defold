package com.dynamo.cr.goeditor;

import java.lang.reflect.Field;

import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.ui.plugin.AbstractUIPlugin;

import com.google.inject.AbstractModule;
import com.google.inject.MembersInjector;
import com.google.inject.TypeLiteral;
import com.google.inject.matcher.Matchers;
import com.google.inject.spi.TypeEncounter;
import com.google.inject.spi.TypeListener;

public class InjectImageModule extends AbstractModule {
    private ImageRegistry imageRegistry;
    private String pluginId;

    class ImageInjector<T> implements MembersInjector<T> {
        private final Field field;
        private String path;

        ImageInjector(Field field, String path) {
            this.field = field;
            this.path = path;
            field.setAccessible(true);
        }

        public void injectMembers(T t) {
            try {
                if (imageRegistry.get(path) == null) {
                    ImageDescriptor imageDesc = AbstractUIPlugin.imageDescriptorFromPlugin(pluginId, path);
                    imageRegistry.put(path, imageDesc);
                }
                field.set(t, imageRegistry.get(path));
            } catch (IllegalAccessException e) {
                throw new RuntimeException(e);
            }
        }
    }

    public InjectImageModule(String pluginId, ImageRegistry imageRegistry) {
        this.pluginId = pluginId;
        this.imageRegistry = imageRegistry;
    }

    @Override
    protected void configure() {

        bindListener(Matchers.any(), new TypeListener() {
            @Override
            public <I> void hear(TypeLiteral<I> literal, TypeEncounter<I> encounter) {
                Field[] fields = literal.getRawType().getDeclaredFields();
                for (Field field : fields) {
                    if (field.isAnnotationPresent(InjectImage.class)) {
                        InjectImage injectImage = field.getAnnotation(InjectImage.class);

                        String path = injectImage.value();
                        ImageDescriptor imageDesc = AbstractUIPlugin.imageDescriptorFromPlugin(pluginId, path);
                        if (imageDesc == null) {
                            encounter.addError("Image '%s' not found", path);
                        } else {
                            encounter.register(new ImageInjector<I>(field, path));
                        }
                    }
                }
            }
        });
    }
}
