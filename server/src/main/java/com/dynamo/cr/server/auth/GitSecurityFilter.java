package com.dynamo.cr.server.auth;

import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.servlet.*;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;

/**
 * A security filter that protects git resources by implementing HTTP Basic Authentication. The filter ensures that the
 * user is authenticated and has access to the requested project/repository.
 */
public class GitSecurityFilter implements Filter {
    private static final Logger LOGGER = LoggerFactory.getLogger(GitSecurityFilter.class);
    private static final String REALM = "HTTP GIT authentication";

    private final EntityManagerFactory entityManagerFactory;
    private final AccessTokenAuthenticator accessTokenAuthenticator;

    public GitSecurityFilter(EntityManagerFactory entityManagerFactory,
                             AccessTokenAuthenticator accessTokenAuthenticator) {
        this.entityManagerFactory = entityManagerFactory;
        this.accessTokenAuthenticator = accessTokenAuthenticator;
    }

    @Override
    public void destroy() {
    }

    private User authenticate(HttpServletRequest request, HttpServletResponse response) throws IOException {
        String authorizationHeader = request.getHeader("Authorization");

        if (isHttpBasicAuthentication(authorizationHeader)) {
            UserCredentials userCredentials = parseAuthorizationHeader(authorizationHeader);

            if (userCredentials == null) {
                response.sendError(HttpServletResponse.SC_BAD_REQUEST);
                return null;
            }

            EntityManager em = entityManagerFactory.createEntityManager();

            User user = ModelUtil.findUserByEmail(em, userCredentials.username);

            if (user != null) {
                // First, try to authenticate as access token.
                if (accessTokenAuthenticator.authenticate(user, userCredentials.password, request.getRemoteAddr())) {
                    return user;
                }
                // Try to authenticate as password.
                if (user.authenticate(userCredentials.password)) {
                    return user;
                }
            }
        }

        LOGGER.debug("Authentication failed (Authorization header: {})", authorizationHeader);
        response.setHeader("WWW-Authenticate", "Basic realm=\"" + REALM + "\"");
        response.sendError(HttpServletResponse.SC_UNAUTHORIZED);
        return null;
    }

    private boolean isHttpBasicAuthentication(String authorizationHeader) {
        return authorizationHeader != null && authorizationHeader.startsWith("Basic ");
    }

    private UserCredentials parseAuthorizationHeader(String authorizationHeader) {
        authorizationHeader = authorizationHeader.substring("Basic ".length());
        String[] values = Base64.base64Decode(authorizationHeader).split(":");

        if (values.length >= 2) {
            String username = values[0];
            String password = values[1];
            return new UserCredentials(username, password);
        }

        return null;
    }

    private boolean verifyRole(User user, long projectId) {
        for (Project p : user.getProjects()) {
            if (p.getId() == projectId)
                return true;
        }
        return false;
    }

    @Override
    public void doFilter(ServletRequest request, ServletResponse response,
                         FilterChain chain) throws IOException, ServletException {
        HttpServletRequest req = (HttpServletRequest) request;
        HttpServletResponse res = (HttpServletResponse) response;

        // NOTE: The first element will be "" after split due to leading "/"
        String[] pathList = req.getPathInfo().split("/");
        if (pathList.length < 3) {
            res.sendError(HttpServletResponse.SC_BAD_REQUEST);
        }

        long projectId = Long.parseLong(pathList[2]);
        User user = authenticate(req, res);

        if (user != null) {
            // NOTE: if user == null sendError is set from authenticate
            // This could be improved
            if (verifyRole(user, projectId)) {
                // Successful. Continue filter-chain
                chain.doFilter(request, response);
            } else {
                res.sendError(HttpServletResponse.SC_FORBIDDEN);
            }
        }
    }

    @Override
    public void init(FilterConfig config) throws ServletException {
    }

    private class UserCredentials {
        final String username;
        final String password;

        private UserCredentials(String username, String password) {
            this.username = username;
            this.password = password;
        }
    }
}
