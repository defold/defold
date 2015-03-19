package com.dynamo.cr.server;

import java.util.HashSet;
import java.util.Set;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.sun.jersey.api.core.DefaultResourceConfig;

public class ResourceConfig extends DefaultResourceConfig {

    protected static Logger logger = LoggerFactory.getLogger(ResourceConfig.class);

    public ResourceConfig() {
    }

    @Override
    public Set<Class<?>> getClasses() {
        Set<Class<?>> classes = new HashSet<Class<?>>();

        ClassLoader cl = this.getClass().getClassLoader();
        try {
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.RepositoryResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.ProjectResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.ProjectsResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.UsersResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.LoginResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.LoginOAuthResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.ProspectsResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.ProductsResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.ChargifyResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.NewsListResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.StatusResource"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.GitExceptionMapper"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.IOExceptionMapper"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.RollbackExceptionMapper"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.WebApplicationExceptionMapper"));
            classes.add(cl.loadClass("com.dynamo.cr.server.ServerExceptionMapper"));
            classes.add(cl.loadClass("com.dynamo.cr.server.auth.AuthenticationExceptionMapper"));
            classes.add(cl.loadClass("com.dynamo.cr.server.auth.AuthorizationExceptionMapper"));

            classes.add(cl.loadClass("com.dynamo.cr.common.providers.ProtobufProviders"));
            classes.add(cl.loadClass("com.dynamo.cr.common.providers.ProtobufProviders$ProtobufMessageBodyWriter"));
            classes.add(cl.loadClass("com.dynamo.cr.common.providers.ProtobufProviders$ProtobufMessageBodyReader"));

            classes.add(cl.loadClass("com.dynamo.cr.common.providers.JsonProviders"));
            classes.add(cl.loadClass("com.dynamo.cr.common.providers.JsonProviders$ProtobufMessageBodyWriter"));
            classes.add(cl.loadClass("com.dynamo.cr.common.providers.JsonProviders$ProtobufMessageBodyReader"));

        } catch (ClassNotFoundException e) {
            logger.error(e.getMessage(), e);
            throw new RuntimeException(e);
        }
        return classes;
    }
}
