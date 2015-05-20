package com.dynamo.cr.server.auth;

import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.security.Principal;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.SecurityContext;
import javax.ws.rs.core.UriInfo;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.MDC;

import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.sun.jersey.api.container.MappableContainerException;
import com.sun.jersey.spi.container.ContainerRequest;
import com.sun.jersey.spi.container.ContainerRequestFilter;


public class SecurityFilter implements ContainerRequestFilter {

    protected static Logger logger = LoggerFactory.getLogger(SecurityFilter.class);

    @Context
    UriInfo uriInfo;

    private static final String REALM = "HTTPS Example authentication";

    @Inject
    private EntityManagerFactory emf;

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
        String authToken = request.getHeaderValue("X-Auth");
        String email = request.getHeaderValue("X-Email");

        if (request.getMethod().equals("OPTIONS")) {
            // Skip authentication for method OPTION
            // Returning "HTTP Basic Authentication is required" will confuse
            // some web browser when performing cross-site requests
            // HTTP Basic Authentication is only intended for the editor (for now at least)
            return null;
        }

        EntityManager em = emf.createEntityManager();

        if (authToken != null && email != null) {
            try {
                email = URLDecoder.decode(email, "UTF-8");
                User user = ModelUtil.findUserByEmail(em, email);
                if (user != null && AuthToken.auth(email, authToken)) {
                    return user;
                } else {
                    throw new MappableContainerException(new AuthenticationException("Invalid username or password", null));
                }

            } catch (UnsupportedEncodingException e) {
                throw new RuntimeException(e);
            }
        }

        // Extract authentication credentials
        String authentication = request
                .getHeaderValue(ContainerRequest.AUTHORIZATION);

        if (authentication != null && authentication.startsWith("Basic ")) {
            /*
             * Http Authentication is only used for the tests so we keep it for now
             * NOTE: HTTP Basic only works if provied. No authorization request is returned to the
             * user. See below about anonymous access
             * We used to "throw new MappableContainerException(new AuthenticationException(
             *                                                  "Authentication credentials are required", REALM));
             */

            authentication = authentication.substring("Basic ".length());
            String[] values = new String(Base64.base64Decode(authentication))
                    .split(":");
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
            } else if (user != null && AuthToken.auth(username, password)) {
                // NOTE: This is rather messy. We also support
                // auth-cookie login using http basic auth.
                // Should perhaps deprecate X-Auth and use basic auth for
                // everything. The reason for this is that the gitsrv
                // must handle both passwords and cookies as passwords
                // are used in tests
                return user;
            } else {
                logger.warn("User authentication failed");
                throw new MappableContainerException(new AuthenticationException(
                        "Invalid username or password", REALM));
            }
        } else {
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
    }

    public class Authorizer implements SecurityContext {

        private User user;
        private Principal principal;

        public Authorizer(final User user) {
            this.user = user;
            this.principal = new UserPrincipal(user);
        }

        @Override
        public Principal getUserPrincipal() {
            return this.principal;
        }

        @Override
        public boolean isUserInRole(String role) {
            if (role.equals("admin")) {
                return user.getRole() == Role.ADMIN;
            }
            else if (role.equals("owner")) {
                long projectId = Long.parseLong(uriInfo.getPathParameters().get("project").get(0));
                for (Project p : user.getProjects()) {
                    if (p.getId().longValue() == projectId && p.getOwner().getId() == user.getId())
                        return true;
                }
            }
            else if (role.equals("member")) {
                long projectId = Long.parseLong(uriInfo.getPathParameters().get("project").get(0));
                for (Project p : user.getProjects()) {
                    if (p.getId().longValue() == projectId)
                        return true;
                }
            }
            else if (role.equals("user")) {
                return user.getRole() == Role.USER || user.getRole() == Role.ADMIN;
            }
            else if (role.equals("anonymous")) {
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
