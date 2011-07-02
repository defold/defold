package com.dynamo.cr.server.auth;

import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.security.Principal;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.SecurityContext;
import javax.ws.rs.core.UriInfo;

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

    @Context
    UriInfo uriInfo;
    private static final String REALM = "HTTPS Example authentication";

    @Context
    private EntityManagerFactory emf;

    public ContainerRequest filter(ContainerRequest request) {
        if (!request.getAbsolutePath().getPath().equals("/login")) {
            // Only authenticate users for paths != /login
            User user = authenticate(request);
            request.setSecurityContext(new Authorizer(user));
        }
        return request;
    }

    private User authenticate(ContainerRequest request) {
        String authCookie = request.getHeaderValue("X-Auth");
        String email = request.getHeaderValue("X-Email");
        System.out.println(authCookie);

        if (request.getMethod().equals("OPTIONS")) {
            // Skip authentication for method OPTION
            // Returning that HTTP Basic Auth. is required will confuse
            // some web browser when performing cross-site requests
            // HTTP Basic Auth. is only intended for the editor (for now at least)
            return null;
        }

        EntityManager em = emf.createEntityManager();

        if (authCookie != null && email != null) {
            try {
                email = URLDecoder.decode(email, "UTF-8");
                User user = ModelUtil.findUserByEmail(em, email);
                if (user != null && AuthCookie.auth(email, authCookie)) {
                    System.out.println("DET FUNKAR!!!");
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
            else if (role.equals("self")) {
                return uriInfo.getPathParameters().get("user").get(0).equals(user.getId().toString());
            }
            else if (role.equals("member")) {
                long projectId = Long.parseLong(uriInfo.getPathParameters().get("project").get(0));
                for (Project p : user.getProjects()) {
                    if (p.getId().longValue() == projectId)
                        return true;
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
