package com.dynamo.cr.server.resources;

import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.auth.DiscourseSSOAuthenticator;
import com.dynamo.cr.server.model.User;

import javax.annotation.security.RolesAllowed;
import javax.inject.Inject;
import javax.ws.rs.GET;
import javax.ws.rs.Path;
import javax.ws.rs.QueryParam;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.UriBuilder;
import java.net.URI;

@Path("discourse")
@RolesAllowed(value = {"user"})
public class DiscourseSSOResource extends BaseResource {

    private final DiscourseSSOAuthenticator authenticator;
    private final String discourseUrl;

    @Inject
    public DiscourseSSOResource(Configuration configuration) {
        this.authenticator = new DiscourseSSOAuthenticator(configuration.getDiscourseSsoKey());
        this.discourseUrl = configuration.getDiscourseUrl();
    }

    /**
     * Authenticates Discourse SSO authentication requesta and redirects back to Discourse.
     *
     * @param sso Request data.
     * @param sig Request signature.
     * @return Redirect back to Discourse.
     */
    @GET
    @Path("sso")
    public Response sso(@QueryParam("sso") String sso, @QueryParam("sig") String sig) {

        if (authenticator.verifySignedRequest(sso, sig)) {

            User user = getUser();

            String responsePayload = authenticator.createResponsePayload(
                    sso,
                    user.getFirstName() + " " + user.getLastName(),
                    user.getEmail(),
                    user.getEmail(),
                    user.getId().toString(),
                    false
            );

            String responseSignature = authenticator.generateSignature(responsePayload);

            URI redirectURI = UriBuilder
                    .fromPath(discourseUrl)
                    .path("sso_login")
                    .queryParam("sso", responsePayload)
                    .queryParam("sig", responseSignature)
                    .build();

            return Response.temporaryRedirect(redirectURI).build();
        }

        throw new WebApplicationException(Response.Status.BAD_REQUEST);
    }
}
