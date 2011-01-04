package com.dynamo.cr.server.resources;

import java.lang.reflect.Type;

import javax.ws.rs.core.Context;
import javax.ws.rs.ext.Provider;

import com.dynamo.cr.server.Server;
import com.sun.jersey.api.core.HttpContext;
import com.sun.jersey.core.spi.component.ComponentContext;
import com.sun.jersey.core.spi.component.ComponentScope;
import com.sun.jersey.server.impl.inject.AbstractHttpContextInjectable;
import com.sun.jersey.spi.inject.Injectable;
import com.sun.jersey.spi.inject.InjectableProvider;

@Provider
public class ServerProvider
      extends AbstractHttpContextInjectable<Server>
      implements InjectableProvider<Context, Type> {

    @Override
    public Injectable<?> getInjectable(ComponentContext ic, Context a, Type c) {
        if (c.equals(Server.class)) {
            return this;
        }

        return null;
    }

    @Override
    public ComponentScope getScope() {
        return ComponentScope.PerRequest;
    }

    // TODO: Get rid of this SINGLELOL!!
    public static Server server;

    @Override
    public Server getValue(HttpContext c) {
        return server;
    }
}
