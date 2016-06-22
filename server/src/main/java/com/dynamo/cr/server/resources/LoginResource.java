package com.dynamo.cr.server.resources;

import com.dynamo.cr.protocol.proto.Protocol.LoginInfo;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.auth.AccessTokenAuthenticator;
import com.dynamo.cr.server.auth.OAuthAuthenticator;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.providers.ProtobufProviders;
import com.dynamo.inject.persist.Transactional;

import javax.inject.Inject;
import javax.servlet.http.HttpServletRequest;
import javax.ws.rs.GET;
import javax.ws.rs.Path;
import javax.ws.rs.core.*;
import javax.ws.rs.core.Response.Status;
import java.util.List;

@Path("/login")
public class LoginResource extends BaseResource {

    @Inject
    protected OAuthAuthenticator authenticator;

    @Inject
    private AccessTokenAuthenticator accessTokenAuthenticator;

    @GET
    @Transactional
    public Response login(@Context HttpHeaders headers, @Context HttpServletRequest request) {

        String email = headers.getRequestHeaders().getFirst("X-Email");
        String password = headers.getRequestHeaders().getFirst("X-Password");

        if (email == null || password == null) {
            throw new ServerException(Status.BAD_REQUEST.toString(), Status.BAD_REQUEST);
        }

        User user = ModelUtil.findUserByEmail(em, email);
        if (user != null && user.authenticate(password)) {

            String accessToken = accessTokenAuthenticator.createSessionToken(user, request.getRemoteAddr());

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
                    .setAuthToken(accessToken)
                    .setFirstName(user.getFirstName())
                    .setLastName(user.getLastName());

            return Response
                    .status(Status.OK)
                    // TODO: What does "secure" means? the "false" argument
                    .type(type)
                    .cookie(new NewCookie(new Cookie("auth", accessToken, "/", null), "", 60 * 60 * 24, false),
                            new NewCookie(new Cookie("email", email, "/", null), "", 60 * 60 * 24, false))
                    .entity(loginInfoBuilder.build()).build();
        } else {
            throw new ServerException(Status.UNAUTHORIZED.toString(), Status.UNAUTHORIZED);
        }
    }
}
