package com.dynamo.cr.editor.core.inject;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import javax.annotation.PreDestroy;

import com.google.inject.AbstractModule;
import com.google.inject.Module;
import com.google.inject.TypeLiteral;
import com.google.inject.matcher.Matchers;
import com.google.inject.spi.InjectionListener;
import com.google.inject.spi.TypeEncounter;
import com.google.inject.spi.TypeListener;

public final class LifecycleModule extends AbstractModule{

    private List<Object> preDestroyObjects = new ArrayList<Object>();

    private InjectionListener<Object> preDestroyInjectionListener = new InjectionListener<Object>() {
        @Override
        public void afterInjection(Object injectee) {
            preDestroyObjects.add(injectee);
        }
    };

    private Module[] modules;

    public LifecycleModule(Module... modules) {
        this.modules = modules;
    }

    @Override
    final protected void configure() {
        bindListener(Matchers.any(), new TypeListener() {
            @Override
            public <I> void hear(TypeLiteral<I> literal, TypeEncounter<I> encounter) {
                Method[] methods = literal.getRawType().getDeclaredMethods();
                for (Method method : methods) {
                    if (method.isAnnotationPresent(PreDestroy.class)) {
                        encounter.register(preDestroyInjectionListener);
                        break;
                    }
                }
            }
        });

        for (Module module : modules) {
            install(module);
        }
    }

    public void close() {
        Throwable firstException = null;
        for (Object o : preDestroyObjects) {
            Method[] methods = o.getClass().getDeclaredMethods();
            for (Method method : methods) {
                if (method.isAnnotationPresent(PreDestroy.class)) {
                    try {
                        method.setAccessible(true);
                        method.invoke(o);
                    } catch (Throwable e) {
                        e.printStackTrace();
                        if (firstException == null)
                            firstException = e;
                    }
                }
            }
        }
        if (firstException != null)
            throw new RuntimeException(firstException);
    }
}
