package com.dynamo.cr.server.resources;

import com.dynamo.cr.protocol.proto.Protocol.LoginInfo;
import com.dynamo.cr.protocol.proto.Protocol.TokenExchangeInfo;
import com.dynamo.cr.protocol.proto.Protocol.TokenExchangeInfo.Type;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.auth.AccessTokenAuthenticator;
import com.dynamo.cr.server.auth.Identity;
import com.dynamo.cr.server.auth.OAuthAuthenticator;
import com.dynamo.cr.server.auth.OAuthAuthenticator.Authentication;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.services.UserService;
import com.dynamo.inject.persist.Transactional;
import com.google.api.client.auth.oauth2.AuthorizationCodeFlow;
import com.google.api.client.auth.oauth2.AuthorizationCodeRequestUrl;
import com.google.api.client.auth.oauth2.AuthorizationCodeResponseUrl;
import com.google.api.client.googleapis.auth.oauth2.GoogleAuthorizationCodeFlow;
import com.google.api.client.googleapis.auth.oauth2.GoogleTokenResponse;
import com.google.api.client.http.GenericUrl;
import com.google.api.client.http.javanet.NetHttpTransport;
import com.google.api.client.json.jackson.JacksonFactory;
import com.google.api.client.util.store.MemoryDataStoreFactory;
import org.apache.commons.codec.binary.Base64;
import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.DeserializationConfig.Feature;
import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.JsonNodeFactory;
import org.codehaus.jackson.node.ObjectNode;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.inject.Inject;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.ws.rs.*;
import javax.ws.rs.core.*;
import javax.ws.rs.core.Response.Status;
import java.io.IOException;
import java.util.Arrays;
import java.util.Optional;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

@Path("/login/oauth")
public class LoginOAuthResource extends BaseResource {
    private static final Logger LOGGER = LoggerFactory.getLogger(LoginOAuthResource.class);

    @Inject
    protected OAuthAuthenticator authenticator;

    @Inject
    private AccessTokenAuthenticator accessTokenAuthenticator;

    @Inject
    private UserService userService;

    // Lock for the flow. The locking scheme is from the google examples.
    // Uncertain if it's actually required as AuthorizationCodeFlow shoudl be thread safe
    // Better be safe than sorry though
    private final Lock lock = new ReentrantLock();
    private AuthorizationCodeFlow flow;

    private AuthorizationCodeFlow initializeFlow() throws ServletException, IOException {

        // TODO: Couldn't find singleton method (getDefaultInstance)
        JacksonFactory jacksonFactory = new JacksonFactory();

        return new GoogleAuthorizationCodeFlow.Builder(
                new NetHttpTransport(),
                jacksonFactory,
                // TODO: Hardcoded. Put in config
                "1066143962919-so5e8h2fdgdnbj50d39a5jtisujbqous.apps.googleusercontent.com",
                "DCHtUF9OFdWPPbRoWwKTRRmJ",
                Arrays.asList("email", "profile", "openid"))
                .setDataStoreFactory(new MemoryDataStoreFactory())
                .setAccessType("offline").build();
    }

    private static String getRedirectURL(HttpServletRequest req) {
        GenericUrl redirectUrl = new GenericUrl(req.getRequestURL().toString());
        redirectUrl.setRawPath("/login/oauth/google/response/");
        return redirectUrl.build();
    }

    @GET
    @Path("/{provider}")
    public Response oauth(@Context HttpServletRequest req,
                          @Context HttpServletResponse resp,
                          @QueryParam("redirect_to") String redirectTo) throws ServletException, IOException {

        if (redirectTo == null) {
            LOGGER.warn("redirect_to set to null");
            redirectTo = "/";
        }

        lock.lock();
        try {
            if (flow == null) {
                flow = initializeFlow();
            }

            AuthorizationCodeRequestUrl authorizationUrl = flow.newAuthorizationUrl();
            authorizationUrl.setRedirectUri(getRedirectURL(req));
            authorizationUrl.set("prompt", "select_account");
            String loginToken = authenticator.newLoginToken();
            JsonNodeFactory factory = JsonNodeFactory.instance;
            ObjectNode node = new ObjectNode(factory);
            node.put("redirect_to", redirectTo);
            node.put("login_token", loginToken);
            authorizationUrl.setState(new String(Base64.encodeBase64(node.toString().getBytes())));

            // DEF-2040:
            // The Google OAuth sign-in on Windows stopped working on 2016-08-03.
            // This was due the combination of;
            //   - Google stopped accepting old browsers on their OAuth web login.
            //   - The webview/browser component in the editor on Windows was/is running in compatibility mode.
            //
            // We discovered that forwarding the user to the sign in URL through JavaScript instead of the old
            // 'Location'-header method circumvented this "compatibility" mode.
            return Response
                    .ok()
                    .entity("<html>\n" +
                            "<head>\n" +
                            "    <title>Redirecting...</title>\n" +
                            "</head>\n" +
                            "<body>\n" +
                            "<script type='text/javascript'>window.location = '" + authorizationUrl.build() + "';</script>\n" +
                            "</body>\n" +
                            "</html>\n")
                    .type(MediaType.TEXT_HTML)
                    .build();
        } finally {
            lock.unlock();
        }
    }

    @GET
    @Path("/{provider}/response")
    public Response oauthIdResponseGet(@Context HttpHeaders headers,
                                       @Context UriInfo uriInfo,
                                       @Context HttpServletRequest req,
                                       @Context HttpServletResponse resp,
                                       @PathParam("provider") String provider,
                                       @QueryParam("state") String state) throws IOException, ServletException {

        StringBuffer buf = req.getRequestURL();
        if (req.getQueryString() != null) {
            buf.append('?').append(req.getQueryString());
        }
        AuthorizationCodeResponseUrl responseUrl = new AuthorizationCodeResponseUrl(buf.toString());

        // NOTE: "action" is set for client OpenID compatibility
        ObjectMapper mapper = new ObjectMapper();
        mapper.configure(Feature.FAIL_ON_UNKNOWN_PROPERTIES, false);
        JsonNode stateJson = mapper.readTree(Base64.decodeBase64(state));

        String code = responseUrl.getCode();
        String action = "login";
        String loginToken = stateJson.get("login_token").getTextValue();

        if (responseUrl.getError() != null) {
            action = "cancel";
        } else if (code == null) {
            action = "cancel";
        } else {
            lock.lock();
            try {
                if (flow == null) {
                    flow = initializeFlow();
                }

                // NOTE: This is actually a GoogleTokenResponse and the Google JWT is accessible from getIdToken if required
                GoogleTokenResponse response = (GoogleTokenResponse) flow
                        .newTokenRequest(code)
                        .setRedirectUri(getRedirectURL(req))
                        .execute();

                String accessToken = response.getAccessToken();
                authenticator.authenticate(flow.getTransport().createRequestFactory(), loginToken, accessToken);
            } finally {
                lock.unlock();
            }
        }

        String redirectTo = stateJson.get("redirect_to").getTextValue();
        redirectTo = redirectTo.replace("{token}", loginToken);
        redirectTo = redirectTo.replace("{action}", action);

        UriBuilder uriBuilder = UriBuilder.fromUri(redirectTo);

        return Response
                .status(Status.TEMPORARY_REDIRECT)
                .header("Location", uriBuilder.build())
                .type(MediaType.TEXT_PLAIN)
                .entity("").build();
    }

    @GET
    @Path("/exchange/{token}")
    @Transactional
    public TokenExchangeInfo exchangeToken(@Context HttpHeaders headers,
                                           @Context UriInfo uriInfo,
                                           @PathParam("token") String token) {

        Authentication authentication = authenticator.exchange(token);
        if (authentication == null) {
            LOGGER.warn("no authenticaton for token found");
            throw new ServerException(Status.BAD_REQUEST.toString(), Status.BAD_REQUEST);
        }

        Identity identity = authentication.identity;
        TokenExchangeInfo.Builder tokenExchangeInfoBuilder = TokenExchangeInfo.newBuilder()
                .setFirstName(identity.firstName)
                .setLastName(identity.lastName)
                .setEmail(identity.email)
                .setLoginToken(token);

        Optional<User> userOptional = userService.findByEmail(identity.email);

        if (userOptional.isPresent()) {

            // Update name (could have changed, or be missing)
            User user = userOptional.get();
            user.setFirstName(identity.firstName);
            user.setLastName(identity.lastName);
            userService.save(user);

            tokenExchangeInfoBuilder.setType(Type.LOGIN);
            tokenExchangeInfoBuilder.setAuthToken(accessTokenAuthenticator.createSessionToken(user, "TODO"));
            tokenExchangeInfoBuilder.setUserId(user.getId());

        } else {
            tokenExchangeInfoBuilder.setType(Type.SIGNUP);
            userService.signupOAuth(identity.email, identity.firstName, identity.lastName, token);
        }

        return tokenExchangeInfoBuilder.build();
    }

    @PUT
    @Path("/register/{token}")
    @Transactional
    public Response register(@PathParam("token") String token) {
        User user = userService.create(token);

        String authToken = accessTokenAuthenticator.createSessionToken(user, null);

        LoginInfo.Builder loginInfoBuilder = LoginInfo.newBuilder()
                .setEmail(user.getEmail())
                .setUserId(user.getId())
                .setAuthToken(authToken)
                .setFirstName(user.getFirstName())
                .setLastName(user.getLastName());

        return Response
                .ok()
                .entity(loginInfoBuilder.build())
                .type(MediaType.APPLICATION_JSON_TYPE)
                .build();
    }

}
