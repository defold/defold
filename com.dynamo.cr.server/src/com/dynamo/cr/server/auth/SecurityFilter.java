package com.dynamo.cr.server.auth;

import java.security.Principal;

import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.SecurityContext;
import javax.ws.rs.core.UriInfo;

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

    /*
    public SecurityFilter() {
        super();
    }*/

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
        User user = null;

        if (username.equals("john") && password.equals("secret")) {
            user = new User("john", "user");
            //System.out.println("USER 'John Doe' AUTHENTICATED");
        } else if (username.equals("jane") && password.equals("secret")) {
            user = new User("jane", "user");
            //System.out.println("USER 'Jane Doe' AUTHENTICATED");
        } else if (username.equals("admin") && password.equals("adminadmin")) {
            user = new User("admin", "admin");
        } else if (username.equals("chmu") && password.equals("chmu")) {
            user = new User("chmu", "user");
        } else if (username.equals("gube") && password.equals("gube")) {
            user = new User("gube", "user");
        } else if (username.equals("rasv") && password.equals("rasv")) {
            user = new User("rasv", "user");
        } else if (username.equals("freg") && password.equals("freg")) {
            user = new User("freg", "user");
        } else {
            System.out.println("USER NOT AUTHENTICATED");
            throw new MappableContainerException(new AuthenticationException(
                    "Invalid username or password", REALM));
        }
        return user;
    }

    public class Authorizer implements SecurityContext {

        private User user;
        private Principal principal;

        public Authorizer(final User user) {
            this.user = user;
            this.principal = new Principal() {

                public String getName() {
                    return user.username;
                }
            };
        }

        public Principal getUserPrincipal() {
            return this.principal;
        }

        public boolean isUserInRole(String role) {
            return (role.equals(user.role));
        }

        public boolean isSecure() {
            return "https".equals(uriInfo.getRequestUri().getScheme());
        }

        public String getAuthenticationScheme() {
            return SecurityContext.BASIC_AUTH;
        }
    }

    public class User {

        public String username;
        public String role;

        public User(String username, String role) {
            this.username = username;
            this.role = role;
        }
    }
}
