package com.dynamo.cr.server;

import java.util.HashSet;
import java.util.Set;
import java.util.logging.Level;

import com.sun.jersey.api.core.DefaultResourceConfig;

public class ResourceConfig extends DefaultResourceConfig {

    @Override
    public Set<Class<?>> getClasses() {
        Set<Class<?>> classes = new HashSet<Class<?>>();

        ClassLoader cl = this.getClass().getClassLoader();
        try {
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.RepositoryResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.ProjectResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.ProjectsResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.UsersResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.ServerProvider"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.EntityManagerFactoryProvider"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.GitExceptionMapper"));
            classes.add(cl.loadClass("com.dynamo.cr.server.auth.AuthenticationExceptionMapper"));
            classes.add(cl.loadClass("com.dynamo.cr.server.auth.AuthorizationExceptionMapper"));
            classes.add(cl.loadClass("com.dynamo.cr.server.auth.ServerExceptionMapper"));

            classes.add(cl.loadClass("com.dynamo.cr.common.providers.ProtobufProviders"));
            classes.add(cl.loadClass("com.dynamo.cr.common.providers.ProtobufProviders$ProtobufMessageBodyWriter"));
            classes.add(cl.loadClass("com.dynamo.cr.common.providers.ProtobufProviders$ProtobufMessageBodyReader"));


            //Currently disabled. We probably need to add .accept(x-protobuf) to all client invocation in order to ensure
            //that we get x-protobuf and not json

            classes.add(cl.loadClass("com.dynamo.cr.common.providers.JsonProviders"));
            classes.add(cl.loadClass("com.dynamo.cr.common.providers.JsonProviders$ProtobufMessageBodyWriter"));
            classes.add(cl.loadClass("com.dynamo.cr.common.providers.JsonProviders$ProtobufMessageBodyReader"));

        } catch (ClassNotFoundException e) {
            Activator.getLogger().log(Level.SEVERE, e.getMessage(), e);
            return null;
        }
        return classes;
    }
}
