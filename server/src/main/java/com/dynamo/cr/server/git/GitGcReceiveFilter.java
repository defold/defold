package com.dynamo.cr.server.git;

import com.dynamo.cr.proto.Config.Configuration;
import org.eclipse.jgit.lib.Repository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.servlet.*;
import javax.servlet.http.HttpServletRequest;
import java.io.IOException;
import java.time.Duration;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Map;
import java.util.Properties;
import java.util.concurrent.ConcurrentHashMap;

public class GitGcReceiveFilter implements Filter {
    private static final Logger LOGGER = LoggerFactory.getLogger(GitGcReceiveFilter.class);

    private final Configuration configuration;
    private final GitRepositoryManager gitRepositoryManager = new GitRepositoryManager();
    private final Map<String, Instant> repositoryTimestamps = new ConcurrentHashMap<>();

    public GitGcReceiveFilter(Configuration configuration) {
        this.configuration = configuration;
    }

    @Override
    public void doFilter(ServletRequest request, ServletResponse response, FilterChain chain)
            throws IOException, ServletException {

        // Process the call (in a separate thread)
        chain.doFilter(request, response);

        if (isPost(request)) {
            String repositoryRoot = getRepositoryRoot(request);

            Instant now = Instant.now();
            Duration maxGcInterval = Duration.of(configuration.getGitGcInterval(), ChronoUnit.MINUTES);

            // Set repository instant to now if missing or older than GC-interval.
            Instant updatedInstant = repositoryTimestamps.compute(
                    repositoryRoot,
                    (s, lastGc) -> lastGc == null || lastGc.isBefore(now.minus(maxGcInterval)) ? now : lastGc);

            // If instant was set to now (referencing same object), it's time to to a GC.
            if (updatedInstant == now) {
                String expirationDate = configuration.getGitPruneExpiration(); // "2 weeks ago"
                Properties gcStatistics = gitRepositoryManager.garbageCollect(repositoryRoot, expirationDate);

                for (Map.Entry<Object, Object> entry : gcStatistics.entrySet()) {
                    LOGGER.info(String.format("Git gc stats for %s, %s:%s", repositoryRoot, entry.getKey(), entry.getValue()));
                }
            }
        }
    }

    private boolean isPost(ServletRequest request) {
        return "POST".equalsIgnoreCase(((HttpServletRequest) request).getMethod());
    }

    private String getRepositoryRoot(ServletRequest request) throws IOException, ServletException {
        Repository gitRepository = (Repository) request.getAttribute("org.eclipse.jgit.Repository");
        if (gitRepository == null || gitRepository.getDirectory() == null) {
            throw new ServletException("Failed to retrieve repository root.");
        }
        return gitRepository.getDirectory().getCanonicalPath();
    }

    @Override
    public void init(FilterConfig filterConfig) {
    }

    @Override
    public void destroy() {
    }
}
