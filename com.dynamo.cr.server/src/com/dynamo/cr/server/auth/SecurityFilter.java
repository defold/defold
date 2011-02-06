package com.dynamo.cr.server.auth;

import java.security.Principal;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.SecurityContext;
import javax.ws.rs.core.UriInfo;

import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.sun.jersey.api.container.MappableContainerException;
import com.sun.jersey.spi.container.ContainerRequest;
import com.sun.jersey.spi.container.ContainerRequestFilter;


public class SecurityFilter implements ContainerRequestFilter {

    // TODO: This injection doesn't work. We need something like for passwd database
    //@Context
    //protected IResourceRepository repository;

    @Context
    UriInfo uriInfo;
    private static final String REALM = "HTTPS Example authentication";

    @Context
    private EntityManagerFactory emf;

    public ContainerRequest filter(ContainerRequest request) {
        User user = authenticate(request);
        request.setSecurityContext(new Authorizer(user));
        return request;
    }

    private User authenticate(ContainerRequest request) {
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
        EntityManager em = emf.createEntityManager();
        User user = ModelUtil.findUserByEmail(em, username);
        if (user != null && user.authenticate(password)) {
            return user;
        }
        else {
            System.out.println("USER NOT AUTHENTICATED");
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

        public boolean isUserInRole(String role) {
            // NOTE: We will add "dynamic" roles based on the actual project, ie owner-role.
            // The owner-role implicit if the user owns the project in question
            if (role.equals("admin")) {
                return user.getRole() == Role.ADMIN;
            }
            else if (role.equals("user")) {
                return user.getRole() == Role.USER || user.getRole() == Role.ADMIN;
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
