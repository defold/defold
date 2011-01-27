package com.dynamo.cr.server.resources;

import java.lang.reflect.Type;

import javax.persistence.EntityManagerFactory;
import javax.ws.rs.core.Context;
import javax.ws.rs.ext.Provider;

import com.sun.jersey.core.spi.component.ComponentContext;
import com.sun.jersey.core.spi.component.ComponentScope;
import com.sun.jersey.spi.inject.Injectable;
import com.sun.jersey.spi.inject.InjectableProvider;

@Provider
public class EntityManagerFactoryProvider implements
        InjectableProvider<Context, Type>,
        Injectable<EntityManagerFactory> {

    // TODO: Yet another singleton (see ServerProvider.java)
    // We could perhaps use SingletonTypeInjectableProvider...
    public static EntityManagerFactory emf;

    @Override
    public EntityManagerFactory getValue() {
        return emf;
    }

    @Override
    public Injectable<?> getInjectable(ComponentContext ic, Context a,
            Type c) {

        if (c.equals(EntityManagerFactory.class)) {
            return this;
        }
        return null;
    }

    @Override
    public ComponentScope getScope() {
        return ComponentScope.Singleton;
    }

}
