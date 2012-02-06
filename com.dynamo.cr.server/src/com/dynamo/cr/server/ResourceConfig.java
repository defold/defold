package com.dynamo.cr.server;

import java.util.HashSet;
import java.util.Set;

import javax.persistence.EntityManagerFactory;
import javax.ws.rs.core.Context;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.branchrepo.BranchRepository;
import com.sun.jersey.api.core.DefaultResourceConfig;
import com.sun.jersey.spi.inject.SingletonTypeInjectableProvider;

public class ResourceConfig extends DefaultResourceConfig {

    protected static Logger logger = LoggerFactory.getLogger(ResourceConfig.class);

    /*
     * NOTE: Singleton-style here!
     * Couldn't find a good solution for this. This class, ResourceConfig, is instantiated by name
     * and we can therefore not inject the configuration data.
     * We use GrizzlyWebContainerFactory to create the HttpServer as we currently rely on servlet-stuff (HttpServletRequest).
     * GrizzlyServerFactory#createHttpServer accepts a ResourceConfig instance but GrizzlyServerFactory#createHttpServer doesn't
     * run in a servlet-container. If we could remove the dependency on HttpServletRequest we could use GrizzlyServerFactory instead.
     * It should, however, be possible to inject our configuration in the servlet-context or similar though.
     */
    public static Server server;
    public static BranchRepository branchRepository;
    public static EntityManagerFactory emf;

    public ResourceConfig() {
        getSingletons().add(new SingletonTypeInjectableProvider<Context, BranchRepository>(BranchRepository.class, branchRepository) {});
        getSingletons().add(new SingletonTypeInjectableProvider<Context, Server>(Server.class, server) {});
        getSingletons().add(new SingletonTypeInjectableProvider<Context, EntityManagerFactory>(EntityManagerFactory.class, emf) {});
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
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.GitExceptionMapper"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.IOExceptionMapper"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.RollbackExceptionMapper"));
            classes.add(cl.loadClass("com.dynamo.cr.server.resources.WebApplicationExceptionMapper"));
            classes.add(cl.loadClass("com.dynamo.cr.server.ServerExceptionMapper"));
            classes.add(cl.loadClass("com.dynamo.cr.server.BranchRepositoryExceptionMapper"));
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
