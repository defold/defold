package com.dynamo.cr.server;

import java.io.File;
import java.io.IOException;
import java.text.ParseException;
import java.util.Calendar;
import java.util.Map;
import java.util.Properties;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicLong;

import javax.servlet.Filter;
import javax.servlet.FilterChain;
import javax.servlet.FilterConfig;
import javax.servlet.ServletException;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;
import javax.servlet.http.HttpServletRequest;

import org.apache.commons.io.FileUtils;
import org.eclipse.jgit.api.Git;
import org.eclipse.jgit.api.errors.GitAPIException;
import org.eclipse.jgit.api.errors.JGitInternalException;
import org.eclipse.jgit.lib.Repository;
import org.eclipse.jgit.util.GitDateParser;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.proto.Config.Configuration;

public class GitGcReceiveFilter implements Filter {

    protected static Logger logger = LoggerFactory.getLogger(GitGcReceiveFilter.class);

    private Map<String, AtomicLong> repositoryTimestamps = new ConcurrentHashMap<String, AtomicLong>();

    private Configuration configuration;

    GitGcReceiveFilter(Configuration configuration) {
        this.configuration = configuration;
    }

    @Override
    public void doFilter(ServletRequest request, ServletResponse response, FilterChain chain)
            throws IOException, ServletException {

        // Process the call (in a separate thread)
        chain.doFilter(request, response);

        boolean isPost = "POST".equalsIgnoreCase(((HttpServletRequest) request).getMethod());
        if(isPost) {
            Repository gitRepository = (Repository)request.getAttribute("org.eclipse.jgit.Repository");
            if(gitRepository == null || gitRepository.getDirectory() == null) {
                return;
            }

            String repositoryRoot = gitRepository.getDirectory().getCanonicalPath();
            long currentTimestamp = System.currentTimeMillis();
            long maxWaitPeriod = configuration.getGitGcInterval() * 60 * 1000; // minutes to ms
            String expirationDate = configuration.getGitPruneExpiration(); // "2 weeks ago"

            AtomicLong lastTimestamp = repositoryTimestamps.get(repositoryRoot);
            if(lastTimestamp == null) {
                lastTimestamp = addNewTimestamp(repositoryRoot, currentTimestamp);
                if(lastTimestamp == null) {
                    // Another process just added a timestamp
                    return;
                }
            } else if(currentTimestamp - lastTimestamp.longValue() <= maxWaitPeriod) {
                return;
            }

            lastTimestamp.set(currentTimestamp);

            Git git = Git.open(new File(repositoryRoot));
            Properties gcStatistics = null;
            // Prevent multiple gc processes from running simultaneously on same repo
            synchronized(lastTimestamp) {
                gcStatistics = callGitGc(repositoryRoot, expirationDate, git);
            }
            git.close();

            for(Map.Entry<Object, Object> entry : gcStatistics.entrySet()) {
                logger.info(String.format("Git gc stats for %s, %s:%s", repositoryRoot, entry.getKey(), entry.getValue()));
            }
        }
        return;
    }

    private Properties callGitGc(String repositoryRoot, String expirationDate, Git git) throws IOException {
        try {
            return git.gc()
                    .setAggressive(false)
                    .setExpire(GitDateParser.parse(expirationDate, Calendar.getInstance()))
                    .call();
        } catch (GitAPIException e) {
            logger.error(String.format("Could not run garbage collect on %s", repositoryRoot), e);
            e.printStackTrace();

        } catch(JGitInternalException ie) {
            String causeMessage = "";
            Throwable cause = ie.getCause();
            if(cause != null) {
                causeMessage = cause.getMessage();
            }
            if(causeMessage != null && causeMessage.contains("packed-refs")) {
                deleteLockFile(repositoryRoot);
            }

            logger.error(String.format("Could not run garbage collect on %s: %s", repositoryRoot, causeMessage), ie);
            ie.printStackTrace();

        } catch (ParseException pe) {
            logger.error(String.format("Could not parse git gc expiration date: %s", expirationDate), pe);
            pe.printStackTrace();
        }
        return new Properties(); // Empty properties list
    }

    private void deleteLockFile(String repositoryRoot) throws IOException {
        // Assuming this is a bare repo and no other gc process is running
        File refLock = new File(repositoryRoot + File.separator + "packed-refs.lock");
        if(refLock.exists()) {
            logger.error("Removing stale packed-refs lock: " + refLock.getCanonicalPath());
            FileUtils.deleteQuietly(refLock);
        }
    }

    private AtomicLong addNewTimestamp(String repositoryRoot, long currentTimestamp) {
        AtomicLong lastTimestamp = new AtomicLong(currentTimestamp);
        AtomicLong previousTimestamp = repositoryTimestamps.putIfAbsent(repositoryRoot, lastTimestamp);

        if(previousTimestamp != null) {
            // No new timestamp created
            return null;
        } else {
            return lastTimestamp;
        }
    }

    @Override
    public void init(FilterConfig filterConfig) {
    }

    @Override
    public void destroy() {
    }
}