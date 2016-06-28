package com.dynamo.cr.server.resources;

import com.dynamo.cr.protocol.proto.Protocol.LoginInfo;
import com.dynamo.cr.protocol.proto.Protocol.TokenExchangeInfo;
import com.dynamo.cr.protocol.proto.Protocol.TokenExchangeInfo.Type;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.auth.AccessTokenAuthenticator;
import com.dynamo.cr.server.auth.Identity;
import com.dynamo.cr.server.auth.OAuthAuthenticator;
import com.dynamo.cr.server.auth.OAuthAuthenticator.Authentication;
import com.dynamo.cr.server.model.*;
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
import javax.persistence.Query;
import javax.persistence.TypedQuery;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.ws.rs.*;
import javax.ws.rs.core.*;
import javax.ws.rs.core.Response.Status;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

@Path("/login/oauth")
public class LoginOAuthResource extends BaseResource {
    private static final Logger LOGGER = LoggerFactory.getLogger(LoginOAuthResource.class);

    @Inject
    protected OAuthAuthenticator authenticator;

    @Inject
    private AccessTokenAuthenticator accessTokenAuthenticator;

    // Lock for the flow. The locking scheme is from the google examples.
    // Uncertain if it's actually required as AuthorizationCodeFlow shoudl be thread safe
    // Better be safe than sorry though
    private final Lock lock = new ReentrantLock();
    private AuthorizationCodeFlow flow;

    private AuthorizationCodeFlow initializeFlow() throws ServletException,
            IOException {

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

            return Response
                    .status(Status.TEMPORARY_REDIRECT)
                    .header("Location", authorizationUrl.build())
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
                String jwt = response.getIdToken();
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

        User user = ModelUtil.findUserByEmail(em, identity.email);
        if (user != null) {

            // Update name (could have changed, or be missing)
            user.setFirstName(identity.firstName);
            user.setLastName(identity.lastName);
            em.persist(user);

            tokenExchangeInfoBuilder.setType(Type.LOGIN);
            tokenExchangeInfoBuilder.setAuthToken(accessTokenAuthenticator.createSessionToken(user, "TODO"));
            tokenExchangeInfoBuilder.setUserId(user.getId());

        } else {
            tokenExchangeInfoBuilder.setType(Type.SIGNUP);

            // Delete old pending NewUser entries.
            Query deleteQuery = em.createQuery("delete from NewUser u where u.email = :email").setParameter("email", identity.email);
            int nDeleted = deleteQuery.executeUpdate();
            if (nDeleted > 0) {
                LOGGER.info("Removed old NewUser row for {}", identity.email);
            }

            NewUser newUser = new NewUser();
            newUser.setFirstName(identity.firstName);
            newUser.setLastName(identity.lastName);
            newUser.setEmail(identity.email);
            newUser.setLoginToken(token);
            em.persist(newUser);
        }

        return tokenExchangeInfoBuilder.build();
    }

    @PUT
    @Path("/register/{token}")
    @Transactional
    public Response register(@PathParam("token") String token,
                             @QueryParam("key") String key) {

        if (key == null || key.isEmpty()) {
            throw new ServerException("A registration key is required", Status.UNAUTHORIZED);
        }

        int initialInvitationCount;

        String testKey = this.server.getConfiguration().getTestRegistrationKey();
        if (testKey.equals(key)) {
            initialInvitationCount = this.server.getConfiguration().getTestInvitationCount();
        } else {
            TypedQuery<Invitation> q = em.createQuery("select i from Invitation i where i.registrationKey = :key", Invitation.class);
            List<Invitation> lst = q.setParameter("key", key).getResultList();
            if (lst.size() == 0) {
                throw new ServerException("Invalid registration key", Status.UNAUTHORIZED);
            }
            Invitation invitation = lst.get(0);
            initialInvitationCount = invitation.getInitialInvitationCount();
            // Remove invitation
            em.remove(lst.get(0));
        }

        List<NewUser> list = em.createQuery("select u from NewUser u where u.loginToken = :loginToken", NewUser.class).setParameter("loginToken", token).getResultList();
        if (list.size() == 0) {
            LOGGER.error("Unable to find NewUser row for {}", token);
            throw new ServerException("Unable to register.", Status.BAD_REQUEST);
        }

        NewUser newUser = list.get(0);

        // Remove prospect (if registered with different email than invitation
        Prospect p = ModelUtil.findProspectByEmail(em, newUser.getEmail());
        if (p != null) {
            em.remove(p);
        }

        // Generate some random password for OpenID accounts.
        byte[] passwordBytes = new byte[32];
        server.getSecureRandom().nextBytes(passwordBytes);
        String password = Base64.encodeBase64String(passwordBytes);

        User user = new User();
        user.setEmail(newUser.getEmail());
        user.setFirstName(newUser.getFirstName());
        user.setLastName(newUser.getLastName());
        user.setPassword(password);
        em.persist(user);
        InvitationAccount account = new InvitationAccount();
        account.setUser(user);
        account.setOriginalCount(initialInvitationCount);
        account.setCurrentCount(initialInvitationCount);
        em.persist(account);
        em.remove(newUser);
        em.flush();

        LOGGER.info("New user registered: {}", user.getEmail());

        String authToken = accessTokenAuthenticator.createSessionToken(user, null);

        LoginInfo.Builder loginInfoBuilder = LoginInfo.newBuilder()
            .setEmail(user.getEmail())
            .setUserId(user.getId())
            .setAuthToken(authToken)
            .setFirstName(user.getFirstName())
            .setLastName(user.getLastName());

        ModelUtil.subscribeToNewsLetter(em, newUser.getEmail(), newUser.getFirstName(), newUser.getLastName());

        return Response
            .ok()
            .entity(loginInfoBuilder.build())
            .type(MediaType.APPLICATION_JSON_TYPE)
            .build();
    }

}
