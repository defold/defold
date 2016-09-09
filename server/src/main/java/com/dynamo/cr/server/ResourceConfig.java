package com.dynamo.cr.server;

import com.dynamo.cr.server.auth.AuthenticationExceptionMapper;
import com.dynamo.cr.server.providers.JsonProviders;
import com.dynamo.cr.server.providers.ProtobufProviders;
import com.dynamo.cr.server.resources.*;
import com.sun.jersey.api.core.DefaultResourceConfig;

import java.util.HashSet;
import java.util.Set;

public class ResourceConfig extends DefaultResourceConfig {

    @Override
    public Set<Class<?>> getClasses() {
        Set<Class<?>> classes = new HashSet<>();
        classes.add(RepositoryResource.class);
        classes.add(ProjectResource.class);
        classes.add(ProjectsResource.class);
        classes.add(UsersResource.class);
        classes.add(LoginResource.class);
        classes.add(LoginOAuthResource.class);
        classes.add(NewsListResource.class);
        classes.add(StatusResource.class);
        classes.add(IOExceptionMapper.class);
        classes.add(RollbackExceptionMapper.class);
        classes.add(WebApplicationExceptionMapper.class);

        classes.add(ServerExceptionMapper.class);
        classes.add(AuthenticationExceptionMapper.class);

        classes.add(ProtobufProviders.class);
        classes.add(ProtobufProviders.ProtobufMessageBodyWriter.class);
        classes.add(ProtobufProviders.ProtobufMessageBodyReader.class);

        classes.add(JsonProviders.class);
        classes.add(JsonProviders.ProtobufMessageBodyWriter.class);
        classes.add(JsonProviders.ProtobufMessageBodyReader.class);

        return classes;
    }
}
