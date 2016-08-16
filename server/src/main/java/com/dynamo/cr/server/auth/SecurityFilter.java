package com.dynamo.cr.server.auth;

import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.sun.jersey.api.container.MappableContainerException;
import com.sun.jersey.spi.container.ContainerRequest;
import com.sun.jersey.spi.container.ContainerRequestFilter;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.MDC;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.servlet.http.HttpServletRequest;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.*;
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.security.Principal;
import java.util.Map;
import java.util.Objects;

public class SecurityFilter implements ContainerRequestFilter {

    private static final Logger LOGGER = LoggerFactory.getLogger(SecurityFilter.class);
    private static final String REALM = "HTTPS Example authentication";

    @Context
    UriInfo uriInfo;

    @Context
    HttpServletRequest httpServletRequest;

    @Inject
    private EntityManagerFactory emf;

    @Inject
    private AccessTokenAuthenticator accessTokenAuthenticator;

    @Override
    public ContainerRequest filter(ContainerRequest request) {
        MDC.put("userId", Long.toString(-1));
        String path = request.getAbsolutePath().getPath();
        if (!path.equals("/login")
                && !path.startsWith("/login/openid/google")
                && !path.startsWith("/login/oauth/google")
                && !path.startsWith("/login/openid/yahoo")
                && !path.startsWith("/login/openid/exchange") && !path.startsWith("/login/openid/register")
                && !path.startsWith("/login/oauth/exchange") && !path.startsWith("/login/oauth/register")
                && !path.startsWith("/prospects")) {
            // Only authenticate users for paths != /login or != /login/openid or != /prospects
            User user = authenticate(request);
            request.setSecurityContext(new Authorizer(user));
            if (user != null && user.getId() != null /* id is null for anonymous */) {
                MDC.put("userId", Long.toString(user.getId()));
            }
        }

        return request;
    }

    private User authenticate(ContainerRequest request) {

        if (request.getMethod().equals("OPTIONS")) {
            // Skip authentication for method OPTION
            // Returning "HTTP Basic Authentication is required" will confuse
            // some web browser when performing cross-site requests
            // HTTP Basic Authentication is only intended for the editor (for now at least)
            return null;
        }

        EntityManager em = emf.createEntityManager();

        /*
            X-header authentication.
         */
        String authToken = request.getHeaderValue("X-Auth");
        String email = request.getHeaderValue("X-Email");

        if (authToken != null && email != null) {
            try {
                email = URLDecoder.decode(email, "UTF-8");
            } catch (UnsupportedEncodingException e) {
                throw new RuntimeException(e);
            }
            return authenticateAccessToken(email, authToken, em);
        }

        /*
            Basic authentication.
         */
        String authentication = request.getHeaderValue(ContainerRequest.AUTHORIZATION);

        if (authentication != null && authentication.startsWith("Basic ")) {
            /*
             * Http Authentication is only used for the tests so we keep it for now
             * NOTE: HTTP Basic only works if provied. No authorization request is returned to the
             * user. See below about anonymous access
             * We used to "throw new MappableContainerException(new AuthenticationException(
             *                                                  "Authentication credentials are required", REALM));
             */

            authentication = authentication.substring("Basic ".length());
            String[] values = Base64.base64Decode(authentication).split(":");
            if (values.length < 2) {
                throw new WebApplicationException(400);
                // "Invalid syntax for username and password"
            }
            String username = values[0];
            String password = values[1];
            if ((username == null) || (password == null)) {
                throw new WebApplicationException(400);
                // "Missing username or password"
            }

            // Validate the extracted credentials
            User user = ModelUtil.findUserByEmail(em, username);
            if (user != null && user.authenticate(password)) {
                return user;
            } else if (user != null && accessTokenAuthenticator.authenticate(user, password, httpServletRequest.getRemoteAddr())) {
                // NOTE: This is rather messy. We also support
                // auth-cookie login using http basic auth.
                // Should perhaps deprecate X-Auth and use basic auth for
                // everything. The reason for this is that the gitsrv
                // must handle both passwords and cookies as passwords
                // are used in tests
                return user;
            } else {
                LOGGER.warn("User authentication failed");
                throw new MappableContainerException(new AuthenticationException("Invalid username or password", REALM));
            }
        }

        /*
            Cookie authentication.

            Note: This is used by the Discourse SSO solution in order to authenticate users that are logged in in the
             dashboard.
         */
        Map<String, Cookie> cookies = request.getCookies();
        Cookie emailCookie = cookies.get("email");
        Cookie authCookie = cookies.get("auth");
        if (emailCookie != null && authCookie != null) {
            return authenticateAccessToken(emailCookie.getValue(), authCookie.getValue(), em);
        }

        /*
         * This experimental. Return an anonymous user if not
         * authToken/email header is set. We could perhaps get rid of
         * !path.startsWith("..") above?
         * The first test is to allow anonymous download of the engine using
         * a provided secret key in the url
         */
        User anonymous = new User();
        anonymous.setEmail("anonymous");
        anonymous.setRole(Role.ANONYMOUS);
        return anonymous;
    }

    private User authenticateAccessToken(String email, String token, EntityManager entityManager) {
        User user = ModelUtil.findUserByEmail(entityManager, email);
        if (user != null && accessTokenAuthenticator.authenticate(user, token, httpServletRequest.getRemoteAddr())) {
            return user;
        }
        LOGGER.warn("User authentication failed");
        throw new MappableContainerException(new AuthenticationException("Invalid username or password", REALM));
    }

    private class Authorizer implements SecurityContext {
        private final User user;
        private final Principal principal;

        Authorizer(User user) {
            this.user = user;
            this.principal = new UserPrincipal(user);
        }

        @Override
        public Principal getUserPrincipal() {
            return this.principal;
        }

        @Override
        public boolean isUserInRole(String role) {
            switch (role) {
                case "admin":
                    return user.getRole() == Role.ADMIN;
                case "owner": {
                    long projectId = Long.parseLong(uriInfo.getPathParameters().get("project").get(0));
                    for (Project p : user.getProjects()) {
                        if (p.getId() == projectId && Objects.equals(p.getOwner().getId(), user.getId()))
                            return true;
                    }
                    break;
                }
                case "member": {
                    long projectId = Long.parseLong(uriInfo.getPathParameters().get("project").get(0));
                    for (Project p : user.getProjects()) {
                        if (p.getId() == projectId)
                            return true;
                    }
                    break;
                }
                case "user":
                    return user.getRole() == Role.USER || user.getRole() == Role.ADMIN;
                case "anonymous":
                    return true;
            }
            return false;
        }

        @Override
        public boolean isSecure() {
            return "https".equals(uriInfo.getRequestUri().getScheme());
        }

        @Override
        public String getAuthenticationScheme() {
            return SecurityContext.BASIC_AUTH;
        }
    }
}
