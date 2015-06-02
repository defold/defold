package com.dynamo.cr.server.resources;

import java.util.List;

import javax.inject.Inject;
import javax.ws.rs.GET;
import javax.ws.rs.Path;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.Cookie;
import javax.ws.rs.core.HttpHeaders;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.NewCookie;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.protocol.proto.Protocol.LoginInfo;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.auth.AuthToken;
import com.dynamo.cr.server.auth.OAuthAuthenticator;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.providers.ProtobufProviders;

@Path("/login")
public class LoginResource extends BaseResource {

    @Inject
    protected OAuthAuthenticator authenticator;

    protected static Logger logger = LoggerFactory.getLogger(LoginResource.class);

    @GET
    public Response login(@Context HttpHeaders headers) {

        String email = headers.getRequestHeaders().getFirst("X-Email");
        String password = headers.getRequestHeaders().getFirst("X-Password");

        if (email == null || password == null) {
            throw new ServerException(Status.BAD_REQUEST.toString(), Status.BAD_REQUEST);
        }

        User user = ModelUtil.findUserByEmail(em, email);
        if (user != null && user.authenticate(password)) {

            String cookie = AuthToken.login(email);

            MediaType type;
            List<MediaType> acceptableMediaTypes = headers.getAcceptableMediaTypes();
            if (acceptableMediaTypes.contains(ProtobufProviders.APPLICATION_XPROTOBUF_TYPE)) {
                type = ProtobufProviders.APPLICATION_XPROTOBUF_TYPE;
            } else if (acceptableMediaTypes.contains(MediaType.APPLICATION_JSON_TYPE)) {
                type = MediaType.APPLICATION_JSON_TYPE;
            } else if (acceptableMediaTypes.contains(MediaType.WILDCARD_TYPE)) {
                // Default to JSON for */*
                type = MediaType.APPLICATION_JSON_TYPE;
            } else {
                throw new ServerException("Unsupported MediaType(s): " + acceptableMediaTypes, Status.BAD_REQUEST);
            }

            LoginInfo.Builder loginInfoBuilder = LoginInfo.newBuilder()
                .setEmail(email)
                .setUserId(user.getId())
                .setAuthToken(cookie)
                .setFirstName(user.getFirstName())
                .setLastName(user.getLastName());

            return Response
                .status(Status.OK)
                // TODO: What does "secure" means? the "false" argument
                .type(type)
                .cookie(new NewCookie(new Cookie("auth", cookie, "/", null), "", 60 * 60 * 24, false),
                        new NewCookie(new Cookie("email", email, "/", null), "", 60 * 60 * 24, false))
                .entity(loginInfoBuilder.build()).build();
        }
        else {
            throw new ServerException(Status.UNAUTHORIZED.toString(), Status.UNAUTHORIZED);
        }
    }

}
