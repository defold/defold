package com.dynamo.cr.server.auth;

import java.io.IOException;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.servlet.Filter;
import javax.servlet.FilterChain;
import javax.servlet.FilterConfig;
import javax.servlet.ServletException;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;

public class GitSecurityFilter implements Filter {

    protected static Logger logger = LoggerFactory.getLogger(GitSecurityFilter.class);

    private EntityManagerFactory emf;

    private static final String REALM = "HTTP GIT authentication";

    public GitSecurityFilter(EntityManagerFactory emf) {
        this.emf = emf;
    }

    @Override
    public void destroy() {
    }

    private User authenticate(FilterChain chain, HttpServletRequest request, HttpServletResponse response) throws IOException {
        /*
         * About OpenID authentication here
         * We don't use the custom fields X-Auth and X-Email used
         * in the REST-protocol. It's currently not possible to set
         * customer headers in client JGit. Therefore we use HTTP Basic Authentication
         * header instead.
         */

        String authentication = request.getHeader("Authorization");
        if (authentication == null) {
            response.setHeader("WWW-Authenticate", "Basic realm=\"" + REALM + "\"");
            response.sendError(HttpServletResponse.SC_UNAUTHORIZED);
            return null;
        }
        if (!authentication.startsWith("Basic ")) {
            return null;
            // additional checks should be done here
            // "Only HTTP Basic authentication is supported"
        }
        authentication = authentication.substring("Basic ".length());
        String[] values = new String(Base64.base64Decode(authentication)).split(":");
        if (values.length < 2) {
            response.sendError(HttpServletResponse.SC_BAD_REQUEST);
            return null;
            // "Invalid syntax for username and password"
        }
        String username = values[0];
        String password = values[1];
        if ((username == null) || (password == null)) {
            response.sendError(HttpServletResponse.SC_BAD_REQUEST);
            return null;
            // "Missing username or password"
        }

        EntityManager em = emf.createEntityManager();

        User user = ModelUtil.findUserByEmail(em, username);
        // NOTE: Not X-Auth and X-Email headers. See comment above.
        if (user != null && AuthToken.auth(username, password)) {
            return user;
        }

        // Validate the extracted credentials
        if (user != null && user.authenticate(password)) {
            return user;
        }
        else {
            logger.warn("User authentication failed");
            response.sendError(HttpServletResponse.SC_UNAUTHORIZED);
            return null;
        }
    }

    private boolean verifyRole(User user, long projectId) {
        for (Project p : user.getProjects()) {
            if (p.getId().longValue() == projectId)
                return true;
        }
        return false;
    }

    @Override
    public void doFilter(ServletRequest request, ServletResponse response,
            FilterChain chain) throws IOException, ServletException {
        HttpServletRequest req = (HttpServletRequest)request;
        HttpServletResponse res = (HttpServletResponse)response;

        // NOTE: The first element will be "" after split due to leading "/"
        String[] pathList = req.getPathInfo().split("/");
        if (pathList.length < 3) {
            res.sendError(HttpServletResponse.SC_BAD_REQUEST);
        }

        long projectId = Long.parseLong(pathList[2]);
        User user = authenticate(chain, req, res);

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

}
