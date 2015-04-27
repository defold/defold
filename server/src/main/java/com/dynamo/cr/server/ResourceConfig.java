package com.dynamo.cr.server;

import java.util.HashSet;
import java.util.Set;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.server.auth.AuthenticationExceptionMapper;
import com.dynamo.cr.server.auth.AuthorizationExceptionMapper;
import com.dynamo.cr.server.providers.JsonProviders;
import com.dynamo.cr.server.providers.ProtobufProviders;
import com.dynamo.cr.server.resources.IOExceptionMapper;
import com.dynamo.cr.server.resources.LoginOAuthResource;
import com.dynamo.cr.server.resources.LoginResource;
import com.dynamo.cr.server.resources.NewsListResource;
import com.dynamo.cr.server.resources.ProjectResource;
import com.dynamo.cr.server.resources.ProjectsResource;
import com.dynamo.cr.server.resources.ProspectsResource;
import com.dynamo.cr.server.resources.RepositoryResource;
import com.dynamo.cr.server.resources.RollbackExceptionMapper;
import com.dynamo.cr.server.resources.StatusResource;
import com.dynamo.cr.server.resources.UsersResource;
import com.dynamo.cr.server.resources.WebApplicationExceptionMapper;
import com.sun.jersey.api.core.DefaultResourceConfig;

public class ResourceConfig extends DefaultResourceConfig {

    protected static Logger logger = LoggerFactory.getLogger(ResourceConfig.class);

    @Override
    public Set<Class<?>> getClasses() {
        Set<Class<?>> classes = new HashSet<Class<?>>();
        classes.add(RepositoryResource.class);
        classes.add(ProjectResource.class);
        classes.add(ProjectsResource.class);
        classes.add(UsersResource.class);
        classes.add(LoginResource.class);
        classes.add(LoginOAuthResource.class);
        classes.add(ProspectsResource.class);
        classes.add(NewsListResource.class);
        classes.add(StatusResource.class);
        classes.add(IOExceptionMapper.class);
        classes.add(RollbackExceptionMapper.class);
        classes.add(WebApplicationExceptionMapper.class);

        classes.add(ServerExceptionMapper.class);
        classes.add(AuthenticationExceptionMapper.class);
        classes.add(AuthorizationExceptionMapper.class);

        classes.add(ProtobufProviders.class);
        classes.add(ProtobufProviders.ProtobufMessageBodyWriter.class);
        classes.add(ProtobufProviders.ProtobufMessageBodyReader.class);

        classes.add(JsonProviders.class);
        classes.add(JsonProviders.ProtobufMessageBodyWriter.class);
        classes.add(JsonProviders.ProtobufMessageBodyReader.class);

        return classes;
    }
}
