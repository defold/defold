package com.dynamo.cr.server.resources;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.ws.rs.GET;
import javax.ws.rs.Path;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.Cookie;
import javax.ws.rs.core.HttpHeaders;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.NewCookie;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.ResponseBuilder;
import javax.ws.rs.core.Response.Status;

import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.auth.AuthCookie;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.User;

@Path("/login")
public class LoginResource extends BaseResource {

    @Context
    private EntityManagerFactory emf;

    @GET
    @Path("")
    public Response login(@Context HttpHeaders headers) throws ServerException {

        ResponseBuilder builder = Response
            .status(Status.INTERNAL_SERVER_ERROR)
            .type(MediaType.TEXT_PLAIN_TYPE);

        String email = headers.getRequestHeaders().getFirst("X-Email");
        String password = headers.getRequestHeaders().getFirst("X-Password");

        if (email == null || password == null) {
            return builder
                .status(Status.BAD_REQUEST)
                .entity(Status.BAD_REQUEST.toString())
                .build();
        }

        EntityManager em = emf.createEntityManager();
        User user = ModelUtil.findUserByEmail(em, email);
        if (user != null && user.authenticate(password)) {

            String cookie = AuthCookie.login(email);

            return builder
                .status(Status.OK)
                // TODO: What does "secure" means? the "false" argument
                .type(MediaType.APPLICATION_JSON_TYPE)
                .cookie(new NewCookie(new Cookie("auth", cookie, "/", "127.0.0.1"), "", 60 * 60 * 24, false),
                        new NewCookie(new Cookie("email", email, "/", "127.0.0.1"), "", 60 * 60 * 24, false))
                .entity(String.format("{ \"auth\" : \"%s\", \"user_id\" : %d }", cookie, user.getId())).build();
        }
        else {
            return builder
                .status(Status.UNAUTHORIZED)
                .entity(Status.UNAUTHORIZED.toString())
                .build();
        }
    }

}
