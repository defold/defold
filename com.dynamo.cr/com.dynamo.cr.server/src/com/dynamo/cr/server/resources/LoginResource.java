package com.dynamo.cr.server.resources;

import java.util.List;

import javax.persistence.Query;
import javax.persistence.TypedQuery;
import javax.servlet.http.HttpServletRequest;
import javax.ws.rs.GET;
import javax.ws.rs.PUT;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.QueryParam;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.Cookie;
import javax.ws.rs.core.HttpHeaders;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.NewCookie;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.ResponseBuilder;
import javax.ws.rs.core.Response.Status;
import javax.ws.rs.core.UriBuilder;
import javax.ws.rs.core.UriInfo;

import org.apache.commons.codec.binary.Base64;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.protocol.proto.Protocol.LoginInfo;
import com.dynamo.cr.protocol.proto.Protocol.TokenExchangeInfo;
import com.dynamo.cr.protocol.proto.Protocol.TokenExchangeInfo.Type;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.auth.AuthToken;
import com.dynamo.cr.server.auth.OpenIDAuthenticator;
import com.dynamo.cr.server.model.Invitation;
import com.dynamo.cr.server.model.InvitationAccount;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.NewUser;
import com.dynamo.cr.server.model.Prospect;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.openid.OpenIDException;
import com.dynamo.cr.server.openid.OpenIDIdentity;
import com.dynamo.inject.persist.Transactional;

@Path("/login")
public class LoginResource extends BaseResource {

    protected static Logger logger = LoggerFactory.getLogger(LoginResource.class);

    private Response badRequest(String message) {
        return Response
            .status(Status.BAD_REQUEST)
            .type(MediaType.TEXT_PLAIN)
            .entity(message)
            .build();
    }

    @GET
    @Path("/openid/{provider}")
    public Response openid(@Context HttpHeaders headers,
                           @Context UriInfo uriInfo,
                           @Context HttpServletRequest request,
                           @PathParam("provider") String provider,
                           @QueryParam("redirect_to") String redirectTo) {

        if (redirectTo == null) {
            String warning = "Missing required query parameter \"redirect_to\"";
            logger.warn(warning);
            return badRequest(warning);
        }

        if (!(provider.equals("google") || provider.equals("yahoo"))) {
            return badRequest("Unsupported OpenID provder");
        }

        OpenIDAuthenticator openIDAuthenticator = server.getOpenIDAuthentication();
        String token = openIDAuthenticator.newLoginToken();

        String host = request.getServerName();
        int port = request.getServerPort();

        String returnTo = UriBuilder.fromUri(String.format("http://%s:%d/login/openid/%s/response", host, port, provider))
                                                   .path(token)
                                                   .queryParam("redirect_to", redirectTo)
                                                   .build().toString();
        String authenticationUrl;
        try {
            authenticationUrl = server.getOpenID().getAuthenticationUrl(provider, String.format("http://%s:%d", host, port), returnTo);
            return Response
                .status(Status.TEMPORARY_REDIRECT)
                .type(MediaType.TEXT_PLAIN)
                .header("Location", authenticationUrl)
                .entity("").build();
        } catch (OpenIDException e) {
            logger.error(e.getMessage(), e);
            return Response
                .status(Status.INTERNAL_SERVER_ERROR)
                .type(MediaType.TEXT_PLAIN)
                .entity(Status.INTERNAL_SERVER_ERROR.toString()).build();
        }
    }

    @GET
    @Path("/openid/{provider}/response/{token}")
    public Response openIdResponseGet(@Context HttpHeaders headers,
                                      @Context UriInfo uriInfo,
                                      @PathParam("provider") String provider,
                                      @PathParam("token") String token,
                                      @QueryParam("redirect_to") String redirectTo) {

        String openIdMode = uriInfo.getQueryParameters().getFirst("openid.mode");

        if (openIdMode == null) {
            return badRequest(Status.BAD_REQUEST.toString());
        }

        OpenIDAuthenticator openIDAuthenticator = server.getOpenIDAuthentication();

        try {

            String action = null;

            if (openIdMode.equals("id_res")) {
                @SuppressWarnings("unused")
                OpenIDIdentity authentication = openIDAuthenticator.authenticate(server.getOpenID(), uriInfo.getRequestUri(), token);
                action = "login";
            } else if (openIdMode.equals("cancel")) {
                action = "cancel";
            } else {
                return badRequest(Status.BAD_REQUEST.toString());
            }

            redirectTo = redirectTo.replace("{token}", token);
            redirectTo = redirectTo.replace("{action}", action);
            UriBuilder uriBilder = UriBuilder.fromUri(redirectTo);

            ResponseBuilder builder = Response
                .status(Status.TEMPORARY_REDIRECT)
                .type(MediaType.TEXT_PLAIN);

            builder.header("Location", uriBilder.build());
            return builder.build();

        } catch (OpenIDException e) {
            logger.error(e.getMessage(), e);
            return badRequest(Status.UNAUTHORIZED.toString());
        }
    }

    @GET
    @Path("/openid/exchange/{token}")
    @Transactional
    public TokenExchangeInfo exchangeToken(@Context HttpHeaders headers,
                                           @Context UriInfo uriInfo,
                                           @PathParam("token") String token) {

        OpenIDAuthenticator openIDAuthenticator = server.getOpenIDAuthentication();
        OpenIDAuthenticator.Authenticator authenticator;
        try {
            authenticator = openIDAuthenticator.exchange(token);
        } catch (OpenIDException e) {
            logger.error(e.getMessage(), e);
            throw new ServerException(Status.BAD_REQUEST.toString(), Status.BAD_REQUEST);
        }

        OpenIDIdentity identity = authenticator.getIdentity();

        TokenExchangeInfo.Builder tokenExchangeInfoBuilder = TokenExchangeInfo.newBuilder()
            .setFirstName(identity.getFirstName())
            .setLastName(identity.getLastName())
            .setEmail(identity.getEmail())
            .setLoginToken(token);

        User user = ModelUtil.findUserByEmail(em, identity.getEmail());
        if (user != null) {

            tokenExchangeInfoBuilder.setType(Type.LOGIN);
            tokenExchangeInfoBuilder.setAuthToken(authenticator.getAuthToken());
            tokenExchangeInfoBuilder.setUserId(user.getId());

        } else {
            tokenExchangeInfoBuilder.setType(Type.SIGNUP);

            // Delete old pending NewUser entries.
            Query deleteQuery = em.createQuery("delete from NewUser u where u.email = :email").setParameter("email", identity.getEmail());
            int nDeleted = deleteQuery.executeUpdate();
            if (nDeleted > 0) {
                logger.info("Removed old NewUser row for {}", identity.getEmail());
            }

            NewUser newUser = new NewUser();
            newUser.setFirstName(identity.getFirstName());
            newUser.setLastName(identity.getLastName());
            newUser.setEmail(identity.getEmail());
            newUser.setLoginToken(token);
            em.persist(newUser);
        }

        return tokenExchangeInfoBuilder.build();
    }

    @PUT
    @Path("/openid/register/{token}")
    @Transactional
    public Response register(@PathParam("token") String token,
                             @QueryParam("key") String key) {

        if (key == null || key.isEmpty()) {
            throw new ServerException("A registration key is required", Status.UNAUTHORIZED);
        }

        int initialInvitationCount = 0;

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
            logger.error("Unable to find NewUser row for {}", token);
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

        logger.info("New user registred: {}", user.getEmail());

        String cookie = AuthToken.login(user.getEmail());

        LoginInfo.Builder loginInfoBuilder = LoginInfo.newBuilder()
            .setEmail(user.getEmail())
            .setUserId(user.getId())
            .setAuthToken(cookie)
            .setFirstName(user.getFirstName())
            .setLastName(user.getLastName());

        ModelUtil.subscribeToNewsLetter(em, newUser.getEmail(), newUser.getFirstName(), newUser.getLastName());

        return Response
            .ok()
            .entity(loginInfoBuilder.build())
            .type(MediaType.APPLICATION_JSON_TYPE)
            .build();
    }

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
