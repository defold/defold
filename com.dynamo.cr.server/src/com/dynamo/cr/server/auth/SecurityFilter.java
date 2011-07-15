package com.dynamo.cr.server.auth;

import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.security.Principal;
import java.util.List;

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

    // TODO: This injection doesn't work. We need something like for passwd database
    //@Context
    //protected IResourceRepository repository;

    protected static Logger logger = LoggerFactory.getLogger(SecurityFilter.class);

    @Context
    UriInfo uriInfo;

    private static final String REALM = "HTTPS Example authentication";

    @Context
    private EntityManagerFactory emf;

    public ContainerRequest filter(ContainerRequest request) {
        MDC.put("userId", Long.toString(-1));
        if (!request.getAbsolutePath().getPath().equals("/login") && !request.getAbsolutePath().getPath().startsWith("/login/openid/google")
                && !request.getAbsolutePath().getPath().startsWith("/login/openid/exchange") && !request.getAbsolutePath().getPath().startsWith("/login/openid/register")) {
            // Only authenticate users for paths != /login or != /login/openid
            User user = authenticate(request);
            request.setSecurityContext(new Authorizer(user));
            if (user != null) {
                MDC.put("userId", Long.toString(user.getId()));
            }
        }

        return request;
    }

    private User authenticate(ContainerRequest request) {
        String authCookie = request.getHeaderValue("X-Auth");
        String email = request.getHeaderValue("X-Email");

        if (request.getMethod().equals("OPTIONS")) {
            // Skip authentication for method OPTION
            // Returning "HTTP Basic Authentication is required" will confuse
            // some web browser when performing cross-site requests
            // HTTP Basic Authentication is only intended for the editor (for now at least)
            return null;
        }

        EntityManager em = emf.createEntityManager();

        if (authCookie != null && email != null) {
            try {
                email = URLDecoder.decode(email, "UTF-8");
                User user = ModelUtil.findUserByEmail(em, email);
                if (user != null && AuthCookie.auth(email, authCookie)) {
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
        if (authentication == null) {
            throw new MappableContainerException(new AuthenticationException(
                    "Authentication credentials are required", REALM));
        }
        if (!authentication.startsWith("Basic ")) {
            return null;
            // additional checks should be done here
            // "Only HTTP Basic authentication is supported"
        }
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
        }
        else {
            logger.warn("User authentication failed");
            throw new MappableContainerException(new AuthenticationException(
                    "Invalid username or password", REALM));
        }
    }

    public class Authorizer implements SecurityContext {

        private User user;
        private Principal principal;

        public Authorizer(final User user) {
            this.user = user;
            this.principal = new Principal() {

                public String getName() {
                    return user.getEmail();
                }
            };
        }

        public Principal getUserPrincipal() {
            return this.principal;
        }

        private boolean affectsOtherUser() {
            List<String> userParams = uriInfo.getPathParameters().get("user");
            if (userParams != null && !userParams.isEmpty()) {
                if (!userParams.get(0).equals(user.getId().toString())) {
                    return true;
                }
            }
            return false;
        }

        public boolean isUserInRole(String role) {
            if (role.equals("admin")) {
                return user.getRole() == Role.ADMIN;
            }
            else if (role.equals("owner")) {
                if (!affectsOtherUser()) {
                    long projectId = Long.parseLong(uriInfo.getPathParameters().get("project").get(0));
                    for (Project p : user.getProjects()) {
                        if (p.getId().longValue() == projectId && p.getOwner().getId() == user.getId())
                            return true;
                    }
                }
            }
            else if (role.equals("member")) {
                if (!affectsOtherUser()) {
                    long projectId = Long.parseLong(uriInfo.getPathParameters().get("project").get(0));
                    for (Project p : user.getProjects()) {
                        if (p.getId().longValue() == projectId)
                            return true;
                    }
                }
            }
            else if (role.equals("user")) {
                if (!affectsOtherUser()) {
                    return user.getRole() == Role.USER || user.getRole() == Role.ADMIN;
                }
            }
            return false;
        }

        public boolean isSecure() {
            return "https".equals(uriInfo.getRequestUri().getScheme());
        }

        public String getAuthenticationScheme() {
            return SecurityContext.BASIC_AUTH;
        }
    }
}
