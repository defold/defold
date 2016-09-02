package com.dynamo.cr.server.resources;

import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.auth.AccessTokenAuthenticator;
import com.dynamo.cr.server.auth.DiscourseSSOAuthenticator;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.services.UserService;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.inject.Inject;
import javax.servlet.http.HttpServletRequest;
import javax.ws.rs.CookieParam;
import javax.ws.rs.GET;
import javax.ws.rs.Path;
import javax.ws.rs.QueryParam;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.UriBuilder;
import java.net.URI;
import java.util.Optional;

@Path("discourse")
public class DiscourseSSOResource extends BaseResource {
    private static final Logger LOGGER = LoggerFactory.getLogger(DiscourseSSOResource.class);
    private final DiscourseSSOAuthenticator authenticator;
    private final AccessTokenAuthenticator accessTokenAuthenticator;
    private final UserService userService;
    private final String discourseUrl;

    @Inject
    public DiscourseSSOResource(Configuration configuration, AccessTokenAuthenticator accessTokenAuthenticator,
                                UserService userService) {
        this.authenticator = new DiscourseSSOAuthenticator(configuration.getDiscourseSsoKey());
        this.discourseUrl = configuration.getDiscourseUrl();
        this.accessTokenAuthenticator = accessTokenAuthenticator;
        this.userService = userService;
    }

    /**
     * Authenticates Discourse SSO authentication request and redirects back to Discourse
     * (or login page if not authenticated).
     *
     * @param sso Request data.
     * @param sig Request signature.
     * @return Redirect back to Discourse if authenticated, redirected to login page otherwise.
     */
    @GET
    @Path("sso")
    public Response sso(
            @QueryParam("sso") String sso,
            @QueryParam("sig") String sig,
            @CookieParam("email") String email,
            @CookieParam("auth") String authToken,
            @Context HttpServletRequest httpServletRequest) {

        if (email != null && authToken != null) {

            Optional<User> userOptional = authenticate(email, authToken, httpServletRequest.getRemoteAddr());

            if (userOptional.isPresent()) {

                if (authenticator.verifySignedRequest(sso, sig)) {
                    User user = userOptional.get();

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

                    LOGGER.info("User {} authenticated for Discourse.", user.getEmail());
                    return Response.temporaryRedirect(redirectURI).build();
                }
            }
        }
        LOGGER.warn("User {} unauthorized for Discourse.", email);
        return loginResponse();
    }

    private static Response loginResponse() {
        URI loginUri = UriBuilder
                .fromPath("http://dashboard.defold.com/oauth/")
                .queryParam("next", "https://forum.defold.com/login")
                .build();
        return Response.temporaryRedirect(loginUri).build();
    }

    private Optional<User> authenticate(String email, String token, String ip) {
        Optional<User> userOptional = userService.findByEmail(email);

        if (userOptional.isPresent()) {
            User user = userOptional.get();
            if (accessTokenAuthenticator.authenticate(user, token, ip)) {
                return Optional.of(user);
            }
        }

        return Optional.empty();
    }
}
