package com.dynamo.cr.server;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.management.ManagementFactory;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URLDecoder;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.ws.rs.core.Response.Status;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.eclipse.jetty.http.HttpHeaders;
import org.eclipse.jetty.http.HttpStatus;
import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.bio.SocketConnector;
import org.eclipse.jetty.server.handler.HandlerList;
import org.eclipse.jetty.server.handler.ResourceHandler;
import org.eclipse.jetty.util.resource.Resource;
import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.eclipse.persistence.jpa.osgi.PersistenceProvider;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.proto.Config;
import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc.Activity;
import com.dynamo.cr.protocol.proto.Protocol.BuildLog;
import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.LaunchInfo;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.cr.protocol.proto.Protocol.ResolveStage;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo.Builder;
import com.dynamo.cr.server.auth.OpenIDAuthenticator;
import com.dynamo.cr.server.auth.SecurityFilter;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.openid.OpenID;
import com.dynamo.cr.server.resources.EntityManagerFactoryProvider;
import com.dynamo.cr.server.resources.ServerProvider;
import com.dynamo.cr.server.util.FileUtil;
import com.dynamo.server.git.CommandUtil;
import com.dynamo.server.git.CommandUtil.Result;
import com.dynamo.server.git.Git;
import com.dynamo.server.git.GitResetMode;
import com.dynamo.server.git.GitStage;
import com.dynamo.server.git.GitState;
import com.dynamo.server.git.GitStatus;
import com.google.protobuf.TextFormat;
import com.sun.grizzly.http.SelectorThread;
import com.sun.jersey.api.NotFoundException;
import com.sun.jersey.api.container.filter.RolesAllowedResourceFilterFactory;
import com.sun.jersey.api.container.grizzly.GrizzlyWebContainerFactory;
import com.sun.jersey.api.core.ResourceConfig;

public class Server implements ServerMBean {

    protected static Logger logger = LoggerFactory.getLogger(Server.class);

    private SelectorThread threadSelector;
    private String baseUri;
    private String branchRoot;
    private Pattern[] filterPatterns;
    private Configuration configuration;
    private EntityManagerFactory emf;
    private org.eclipse.jetty.server.Server jettyServer;
    private ETagCache etagCache = new ETagCache(10000);
    private static final int MAX_ACTIVE_LOGINS = 1024;
    private OpenID openID = new OpenID();
    private OpenIDAuthenticator openIDAuthentication = new OpenIDAuthenticator(MAX_ACTIVE_LOGINS);
    private SecureRandom secureRandom;

    // Stats
    private AtomicInteger resourceDataRequests = new AtomicInteger();
    private AtomicInteger resourceInfoRequests = new AtomicInteger();

    private int cleanupBuildsInterval = 10 * 1000; // 10 seconds
    private int keepBuildDescFor = 100 * 1000; // 100 seconds
    private int nextBuildNumber = 0;
    private Map<Integer, RuntimeBuildDesc> builds = new HashMap<Integer, RuntimeBuildDesc>();

    private CleanBuildsThread cleanupThread;

    class CleanBuildsThread extends Thread {
        private boolean quit = false;

        public CleanBuildsThread() {
            setName("Cleanup builds thread");
        }

        public void run() {
            while (!quit) {
                try {
                    Thread.sleep(cleanupBuildsInterval);

                    Set<Integer> toRemove = new HashSet<Integer>();
                    synchronized (builds) {
                        for (Integer id : builds.keySet()) {
                            RuntimeBuildDesc buildDesc = builds.get(id);
                            long time = System.currentTimeMillis();
                            if (buildDesc.activity == Activity.IDLE && time > (buildDesc.buildCompleted + keepBuildDescFor)) {
                                toRemove.add(id);
                            }
                        }
                        for (Integer id : toRemove) {
                            logger.info("Removing build {}", id);
                            builds.remove(id);
                        }
                    }
                } catch (InterruptedException e) {
                }
            }
        }

        public void quit() {
            this.quit = true;
        }
    }

    /**
     * Clean up for old builds thread wake-up interval
     * @param cleanupBuildsInterval interval in ms
     */
    public void setCleanupBuildsInterval(int cleanupBuildsInterval) {
        this.cleanupBuildsInterval = cleanupBuildsInterval;
        this.cleanupThread.interrupt();
    }

    /**
     * Set time to keep old builds
     * @param keepBuildDescFor time to keep completed builds for in ms
     */
    public void setKeepBuildDescFor(int keepBuildDescFor) {
        this.keepBuildDescFor = keepBuildDescFor;
    }

    public Server(String configuration_file) throws IOException {
        try {
            secureRandom = SecureRandom.getInstance("SHA1PRNG");
        } catch (NoSuchAlgorithmException e1) {
            throw new RuntimeException(e1);
        }

        this.cleanupThread = new CleanBuildsThread();
        this.cleanupThread.start();

        loadConfig(configuration_file);

        assert ServerProvider.server == null;
        ServerProvider.server = this;

        HashMap<String, Object> props = new HashMap<String, Object>();
        props.put(PersistenceUnitProperties.CLASSLOADER, this.getClass().getClassLoader());
        // NOTE: JPA-PersistenceUnits: in plug-in MANIFEST.MF has to be set. Otherwise the persistence unit is not found.
        emf = new PersistenceProvider().createEntityManagerFactory(configuration.getPersistenceUnitName(), props);
        if (emf == null) {
            // TODO: We should not continue...
            logger.error("Persistant unit '{}' not found", configuration.getPersistenceUnitName());
        }
        else {
            bootStrapUsers();
        }

        EntityManagerFactoryProvider.emf = emf;

        baseUri = String.format("http://localhost:%d/", this.configuration.getServicePort());
        final Map<String, String> initParams = new HashMap<String, String>();

        /*
         * NOTE:
         * Class scanning in jersey with "com.sun.jersey.config.property.packages"
         * is not compatible with OSGi.
         * Therefore are all resource classes specified in class ResourceConfig
         * using  "com.sun.jersey.config.property.resourceConfigClass" instead
         *
         * Old "code":
         * initParams.put("com.sun.jersey.config.property.packages", "com.dynamo.cr.server.resources;com.dynamo.cr.server.auth;com.dynamo.cr.common.providers");
         */
        initParams.put("com.sun.jersey.config.property.resourceConfigClass",
                       "com.dynamo.cr.server.ResourceConfig");

        initParams.put(ResourceConfig.PROPERTY_CONTAINER_REQUEST_FILTERS,
                PreLoggingRequestFilter.class.getName() +  ";" + SecurityFilter.class.getName());
        initParams.put(ResourceConfig.PROPERTY_RESOURCE_FILTER_FACTORIES,
                RolesAllowedResourceFilterFactory.class.getName());
        initParams.put(ResourceConfig.PROPERTY_CONTAINER_RESPONSE_FILTERS,
                CrossSiteHeaderResponseFilter.class.getName() + ";" + PostLoggingResponseFilter.class.getName());

        threadSelector = GrizzlyWebContainerFactory.create(baseUri, initParams);

        if (!Git.checkGitVersion()) {
            // TODO: Hmm, exception...
            throw new RuntimeException("Invalid version of git or not found");
        }

        jettyServer = new org.eclipse.jetty.server.Server();

        SocketConnector connector = new SocketConnector();
        connector.setPort(configuration.getBuildDataPort());
        jettyServer.addConnector(connector);
        HandlerList handlerList = new HandlerList();
        ResourceHandler resourceHandler = new ResourceHandler() {

            @Override
            public void handle(String target, Request baseRequest, HttpServletRequest request, HttpServletResponse response) throws IOException, ServletException {
                if (baseRequest.isHandled())
                    return;

                if (target.equals("/__verify_etags__")) {
                    baseRequest.setHandled(true);

                    if (!request.getMethod().equals("POST")) {
                        response.setStatus(HttpServletResponse.SC_BAD_REQUEST );
                        return;
                    }

                    StringBuffer responseBuffer = new StringBuffer();
                    BufferedReader reader = new BufferedReader(new InputStreamReader(request.getInputStream()));

                    try {
                        String line = reader.readLine();
                        while (line != null) {
                            int i = line.indexOf(' ');
                            URI uri;
                            try {
                                uri = new URI(URLDecoder.decode(line.substring(0, i), "UTF-8"));
                                uri = uri.normalize(); // http://foo.com//a -> http://foo.com/a
                            } catch (URISyntaxException e) {
                                logger.warn(e.getMessage(), e);
                                continue;
                            }

                            String etag = line.substring(i + 1);
                            Resource resource = getResource(uri.getPath());
                            if (resource != null && resource.exists() && resource.getFile() != null) {
                                String thisEtag = etagCache.getETag(resource.getFile());
                                if (etag.equals(thisEtag)) {
                                    responseBuffer.append(line.substring(0, i));
                                    responseBuffer.append('\n');
                                }
                            }
                            else {
                                logger.warn("File doesn't exists {}", uri.getPath());
                            }

                            line = reader.readLine();
                        }
                    } finally {
                        reader.close();
                    }

                    response.getWriter().print(responseBuffer);
                    response.setStatus(HttpServletResponse.SC_OK);
                    return;
                }

                String ifNoneMatch = request.getHeader(HttpHeaders.IF_NONE_MATCH);
                if (ifNoneMatch != null) {
                    Resource resource = getResource(request);
                    if (resource != null && resource.exists()) {
                        File file = resource.getFile();
                        if (file != null) {
                            String thisEtag = etagCache.getETag(file);
                            if (thisEtag != null && ifNoneMatch.equals(thisEtag)) {
                                baseRequest.setHandled(true);
                                response.setHeader(HttpHeaders.ETAG, thisEtag);
                                response.setStatus(HttpStatus.NOT_MODIFIED_304);
                                return;
                            }
                        }
                    }
                }

                super.handle(target, baseRequest, request, response);
            }

            @Override
            protected void doResponseHeaders(HttpServletResponse response,
                    Resource resource,
                    String mimeType) {
                super.doResponseHeaders(response, resource, mimeType);
                try {
                    File file = resource.getFile();
                    if (file != null) {
                        String etag = etagCache.getETag(file);
                        if (etag != null) {
                            response.setHeader(HttpHeaders.ETAG, etag);
                        }
                    }
                }
                catch(IOException e) {
                    throw new RuntimeException(e);
                }
            }
        };
        resourceHandler.setResourceBase(configuration.getBranchRoot());
        handlerList.addHandler(resourceHandler);
        jettyServer.setHandler(handlerList);

        try {
            jettyServer.start();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        MBeanServer mbs = ManagementFactory.getPlatformMBeanServer();
        ObjectName name;
        try {
            name = new ObjectName("com.dynamo:type=Server");
            mbs.registerMBean(this, name);
        } catch (Throwable e) {
            logger.error(e.getMessage(), e);
        }
    }

    public SecureRandom getSecureRandom() {
        return secureRandom;
    }

    public OpenID getOpenID() {
        return openID;
    }

    public OpenIDAuthenticator getOpenIDAuthentication() {
        return openIDAuthentication;
    }

    @Override
    public int getETagCacheHits() {
        return etagCache.getCacheHits();
    }

    @Override
    public int getETagCacheMisses() {
        return etagCache.getCacheMisses();
    }

    @Override
    public int getResourceDataRequests() {
        return resourceDataRequests.get();
    }

    @Override
    public int getResourceInfoRequests() {
        return resourceInfoRequests.get();
    }

    private void bootStrapUsers() {
        EntityManager em = emf.createEntityManager();
        em.getTransaction().begin();
        String[][] users = new String[][] {
                { "dynamogameengine@gmail.com", "admin", "Mr", "Admin" } };

        for (String[] entry : users) {
            if (ModelUtil.findUserByEmail(em, entry[0]) == null) {
                User u = new User();
                u.setEmail(entry[0]);
                u.setFirstName(entry[2]);
                u.setLastName(entry[3]);
                if (entry[1].equals("admin")) {
                    u.setPassword("mobEpeyfUj9");
                    u.setRole(Role.ADMIN);
                } else {
                    u.setPassword(entry[1]);
                }
                em.persist(u);
            }
        }
        em.getTransaction().commit();
        em.close();
    }

    void loadConfig(String file_name) throws IOException {

        FileReader fr = new FileReader(file_name);
        Config.Configuration.Builder conf_builder = Config.Configuration.newBuilder();
        TextFormat.merge(fr, conf_builder);

        configuration = conf_builder.build();

        branchRoot = configuration.getBranchRoot();

        filterPatterns = new Pattern[configuration.getFiltersCount()];
        int i = 0;
        for (String f : configuration.getFiltersList()) {
            filterPatterns[i++] = Pattern.compile(f);
        }

    }

    public void stop() {
        assert ServerProvider.server != null;
        ServerProvider.server = null;

        if (this.cleanupThread != null) {
            this.cleanupThread.interrupt();
            this.cleanupThread.quit();
            try {
                this.cleanupThread.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        threadSelector.stopEndpoint();
        try {
            jettyServer.stop();
        } catch (Exception e) {
            logger.error(e.getMessage(), e);
        }
        emf.close();
    }

    public Project getProject(EntityManager em, String id) throws ServerException {
        Project project = em.find(Project.class, Long.parseLong(id));
        if (project == null)
            throw new ServerException(String.format("No such project %s", project));

        return project;
    }

    public User getUser(EntityManager em, String userId) throws ServerException {
        User user = em.find(User.class, Long.parseLong(userId));
        if (user == null)
            throw new ServerException(String.format("No such user %s", userId), javax.ws.rs.core.Response.Status.NOT_FOUND);

        return user;
    }

    public static void main(String[] args) throws IOException {
        Server s = new Server("config/jetbot.config");
        System.in.read();
        s.stop();
        System.exit(0);
    }

    public String getBaseURI() {
        return baseUri;
    }

    void ensureProjectBranch(EntityManager em, String project, String user, String branch) throws ServerException {
        // We check that the project really exists here
        getProject(em, project);

        User u = getUser(em, user);
        String p = String.format("%s/%s/%d/%s", branchRoot, project, u.getId(), branch);
        if (!new File(p).exists()) {
            throw new ServerException(String.format("No such branch %s/%s", user, branch));
        }
    }

    public void createBranch(EntityManager em, String project, String user, String branch) throws ServerException, IOException {
        // We check that the project really exists here
        getProject(em, project);
        User u = getUser(em, user);

        String p = String.format("%s/%s/%d/%s", branchRoot, project, u.getId(), branch);
        File f = new File(p);
        if (f.exists()) {
            throw new ServerException(String.format("Branch %s already exists", p));
        }

        f.getParentFile().mkdirs();

        Git git = new Git();
        String sourcePath = String.format("%s/%s", configuration.getRepositoryRoot(), project);
        git.cloneRepo(sourcePath, p);

        if (configuration.hasBuiltinsDirectory()) {
            String builtins = configuration.getBuiltinsDirectory();
            String dest = String.format("%s/builtins", p);
            // Create symbolic link to builtins. See deleteBranch()
            Result r = CommandUtil.execCommand(null, null, new String[] {"ln", "-s", builtins, dest});
            if (r.exitValue != 0) {
                logger.error(r.stdErr.toString());
                FileUtils.deleteDirectory(new File(p));
                throw new ServerException(String.format("Unable to create branch. Internal server error"));
            }
        }
    }

    public String getBranchRoot() {
        return branchRoot;
    }

    public boolean containsBranch(EntityManager em, String project, String user, String branch) throws ServerException {
        User u = getUser(em, user);
        String p = String.format("%s/%s/%d/%s", branchRoot, project, u.getId(), branch);
        return new File(p).exists();
    }

    public void deleteBranch(EntityManager em, String project, String user, String branch) throws ServerException  {
        ensureProjectBranch(em, project, user, branch);
        User u = getUser(em, user);

        String p = String.format("%s/%s/%d/%s", branchRoot, project, u.getId(), branch);
        try {
            String dest = String.format("%s/builtins", p);
            // Remove symbol link to builtins. See createBranch()
            // We need to remove the symbolic link *before* removeDir is invoked. FileUtil.removeDir follow links...
            CommandUtil.execCommand(null, null, new String[] {"rm", dest});
            FileUtil.removeDir(new File(p));
        } catch (IOException e) {
            throw new ServerException("", e);
        }
    }

    public Protocol.ResourceInfo getResourceInfo(EntityManager em, String project, String user, String branch,
            String path) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        if (path == null)
            throw new ServerException("null path");

        if (path.indexOf("..") != -1) {
            throw new ServerException("Relative paths are not permitted", Status.NOT_FOUND);
        }

        resourceInfoRequests.addAndGet(1);

        User u = getUser(em, user);
        String p = String.format("%s/%s/%d/%s%s", branchRoot, project, u.getId(), branch, path);
        File f = new File(p);

        if (!f.exists())
            throw new NotFoundException(String.format("%s not found", p));

        if (f.isFile()) {
            Builder builder = Protocol.ResourceInfo.newBuilder()
            .setName(f.getName())
            .setPath(String.format("%s", path))
            .setSize((int) f.length())
            .setLastModified(f.lastModified())
            .setType(Protocol.ResourceType.FILE);

            return builder.build();
        }
        else if (f.isDirectory()) {
            Builder builder = Protocol.ResourceInfo.newBuilder()
            .setName(f.getName())
            .setPath(String.format("%s", path))
            .setSize(0)
            .setLastModified(f.lastModified())
            .setType(Protocol.ResourceType.DIRECTORY);

            for (File subf : f.listFiles()) {
                boolean match = false;
                String pathToMatch = path;
                if (pathToMatch.equals("/"))
                    pathToMatch += subf.getName();
                else
                    pathToMatch += "/" + subf.getName();

                for (Pattern pf : filterPatterns) {
                    if (pf.matcher(pathToMatch).matches()) {
                        match = true;
                        break;
                    }
                }
                if (!match)
                    builder.addSubResourceNames(subf.getName());
            }
            return builder.build();
        }
        else {
            throw new ServerException("Unknown resource type: " + f);
        }
    }

    public byte[] getResourceData(EntityManager em, String project, String user, String branch,
            String path, String revision) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        if (path == null)
            throw new ServerException("null path");

        if (path.indexOf("..") != -1) {
            throw new ServerException("Relative paths are not permitted", Status.NOT_FOUND);
        }

        resourceDataRequests.addAndGet(1);

        User u = getUser(em, user);
        String p = String.format("%s/%s/%d/%s%s", branchRoot, project, u.getId(), branch, path);
        if (revision != null && !revision.equals("")) {
            Git git = new Git();
            try {
                return git.show(String.format("%s/%s/%d/%s", branchRoot, project, u.getId(), branch), path.substring(1), revision);
            } catch (IOException e) {
                throw new NotFoundException(String.format("%s (rev '%s') not found", p, revision));
            }
        } else {
            File f = new File(p);

            if (!f.exists())
                throw new NotFoundException(String.format("%s not found", p));
            else if (!f.isFile())
                throw new ServerException("getResourceData opertion is not applicable on non-files");

            byte[] file_data = new byte[(int) f.length()];
            BufferedInputStream is = new BufferedInputStream(new FileInputStream(p));
            is.read(file_data);
            is.close();
            return file_data;
        }
    }

    public void deleteResource(EntityManager em, String project, String user, String branch,
            String path) throws ServerException, IOException {
        ensureProjectBranch(em, project, user, branch);
        if (path == null)
            throw new ServerException("null path");
        User u = getUser(em, user);

        String p = String.format("%s/%s/%d/%s%s", branchRoot, project, u.getId(), branch, path);
        File f = new File(p);
        // NOTE: We need to cache isFile() here.
        boolean isFile = f.isFile();

        if (!f.exists())
            throw new NotFoundException(String.format("%s not found", p));

        boolean recursive = false;
        if (f.isDirectory())
            recursive = true;

        String branch_path = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        Git git = new Git();
        boolean force = true;
        git.rm(branch_path, path.substring(1), recursive, force); // NOTE: Remove / from path

        // The code is somewhat strange
        // We always recreate to parent directory for a file
        // in order to avoid implicit deletion of directories. This behavior confuses
        // eclipse. We also need to remove the directory manually as git won't remove
        // empty directories. (git doesn't track empty directories). This is direct
        // consequence of the first behavior (recreating directories)
        if (isFile) {
            String localPath = FilenameUtils.getFullPath(p);
            File localPathFile = new File(localPath);
            localPathFile.mkdirs();
        } else {
            f.delete();
        }
    }

    public void renameResource(EntityManager em, String project, String user, String branch,
            String source, String destination) throws ServerException, IOException {
        ensureProjectBranch(em, project, user, branch);
        if (source == null)
            throw new ServerException("null source");
        User u = getUser(em, user);

        String source_path = String.format("%s/%s/%d/%s%s", branchRoot, project, u.getId(), branch, source);
        File f = new File(source_path);

        if (!f.exists())
            throw new NotFoundException(String.format("%s not found", source_path));

        String branch_path = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        Git git = new Git();
        git.mv(branch_path, source.substring(1), destination.substring(1), true); // NOTE: Remove / from path
    }

    public void revertResource(EntityManager em, String project, String user, String branch,
            String path) throws IOException, ServerException {
        User u = getUser(em, user);
        String branch_path = String.format("%s/%s/%d/%s", branchRoot, project, u.getId(), branch);
        Git git = new Git();
        GitStatus status = git.getStatus(branch_path);
        String localPath = path.substring(1);
        GitStatus.Entry entry = null;
        for (GitStatus.Entry f : status.files) {
            if (f.file.equals(path.substring(1))) {
                entry = f;
                break;
            }
        }
        if (entry != null) {
            switch (entry.indexStatus) {
            case 'A':
                git.rm(branch_path, localPath, false, true);
                break;
            case 'R':
                git.rm(branch_path, localPath, false, true);
                git.reset(branch_path, GitResetMode.MIXED, entry.original, "HEAD");
                git.checkout(branch_path, entry.original, false);
                break;
            case 'D':
            case 'M':
                git.reset(branch_path, GitResetMode.MIXED, localPath, "HEAD");
            default:
                git.checkout(branch_path, localPath, false);
                break;
            }
        }
    }

    public void putResourceData(EntityManager em, String project, String user, String branch,
            String path, byte[] data) throws ServerException, IOException  {
        ensureProjectBranch(em, project, user, branch);
        if (path == null)
            throw new ServerException("null path");

        String p = String.format("%s/%s/%s/%s%s", branchRoot, project, user, branch, path);
        File f = new File(p);
        File parent = f.getParentFile();

        if (!parent.exists())
            throw new NotFoundException(String.format("Directory %s not found", parent.getPath()));

        FileOutputStream os = new FileOutputStream(f);
        os.write(data);
        os.close();

        String branch_path = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        Git git = new Git();
        try {
            git.add(branch_path, path.substring(1));  // NOTE: Remove / from path
        } catch (Throwable e) {
            // Rollback if git error
            f.delete();
            throw new ServerException("Failed to add file", e);
        }
    }

    public void mkdir(String project, String user, String branch, String path) throws ServerException {
        if (path == null)
            throw new ServerException("null path");

        String p = String.format("%s/%s/%s/%s%s", branchRoot, project, user, branch, path);
        File f = new File(p);
        if (!f.exists()) {
            f.mkdirs();
        }
        else {
            if (!f.isDirectory())
                throw new ServerException(String.format("File %s already exists and is not a directory", p));
        }
    }

    public BranchStatus getBranchStatus(EntityManager em, String project, String user, String branch) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        GitState state = git.getState(p);
        GitStatus status = git.getStatus(p);

        Protocol.BranchStatus.State s;
        if (state == GitState.CLEAN)
            s = Protocol.BranchStatus.State.CLEAN;
        else if (state == GitState.DIRTY)
            s = Protocol.BranchStatus.State.DIRTY;
        else if (state == GitState.MERGE)
            s = Protocol.BranchStatus.State.MERGE;
        else
            throw new RuntimeException("Internal error. Unknown state: " + state);

        BranchStatus.Builder builder = Protocol.BranchStatus
        .newBuilder().setName(branch)
        .setBranchState(s)
        .setCommitsAhead(status.commitsAhead)
        .setCommitsBehind(status.commitsBehind);

        for (GitStatus.Entry e : status.files) {
            String fn = String.format("/%s", e.file);
            BranchStatus.Status.Builder status_builder = BranchStatus.Status.newBuilder().setName(fn).setIndexStatus("" + e.indexStatus).setWorkingTreeStatus("" + e.workingTreeStatus);
            if (e.original != null) {
                status_builder.setOriginal(String.format("/%s", e.original));
            }
            builder.addFileStatus(status_builder);
        }

        return builder.build();
    }

    public String[] getBranchNames(EntityManager em, String project, String user) {
        String p = String.format("%s/%s/%s", branchRoot, project, user);
        File d = new File(p);
        if (!d.isDirectory())
            return new String[] {};

        List<String> list = new ArrayList<String>();
        for (File f : d.listFiles()) {
            if (f.isDirectory()) {
                if (new File(f, ".git").isDirectory()) {
                    list.add(f.getName());
                }
            }
        }

        String[] ret = new String[list.size()];
        list.toArray(ret);

        return ret;
    }

    public void updateBranch(EntityManager em, String project, String user, String branch) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        git.pull(p);
    }

    public CommitDesc commitBranch(EntityManager em, String project, String user, String branch, String message) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        return git.commitAll(p, message);
    }

    public void resolveResource(EntityManager em, String project, String user, String branch, String path,
            ResolveStage stage) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        if (path == null)
            throw new ServerException("null path");

        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        GitStage git_stage;
        if (stage == ResolveStage.BASE) {
            git_stage = GitStage.BASE;
        }
        else if (stage == ResolveStage.YOURS) {
            git_stage = GitStage.YOURS;
        }
        else if (stage == ResolveStage.THEIRS) {
            git_stage = GitStage.THEIRS;
        }
        else {
            throw new RuntimeException("Internal error. Unknown stage: " + stage);
        }

        git.resolve(p, path.substring(1), git_stage);  // NOTE: Remove / from path
    }

    public CommitDesc commitMergeBranch(EntityManager em, String project, String user, String branch,
            String message) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        return git.commit(p, message);
    }

    public void publishBranch(EntityManager em, String project, String user, String branch) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        git.push(p);
    }

    public void reset(EntityManager em, String project, String user, String branch, String mode, String target) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        GitResetMode resetMode = GitResetMode.MIXED;
        if (mode.equals("mixed")) {
            resetMode = GitResetMode.MIXED;
        } else if (mode.equals("soft")) {
            resetMode = GitResetMode.SOFT;
        } else if (mode.equals("hard")) {
            resetMode = GitResetMode.HARD;
        } else if (mode.equals("merge")) {
            resetMode = GitResetMode.MERGE;
        } else if (mode.equals("keep")) {
            resetMode = GitResetMode.KEEP;
        }
        git.reset(p, resetMode, null, target);
    }

    public Log log(EntityManager em, String project, String user, String branch, int maxCount) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        return git.log(p, maxCount);
    }


    static class BuildRunnable implements IBuildRunnable {

        private int id;
        private Server server;
        private String branchRoot;
        private String project;
        private String user;
        private String branch;
        private Pattern workPattern;
		private boolean rebuild;
        private Configuration configuration;
        private StringBuffer stdOut;
        private StringBuffer stdErr;
        private boolean cancel = false;

        public BuildRunnable(Server server, int id, String branch_root, String project, String user, String branch, boolean rebuild) {
            this.id = id;
            this.server = server;
            this.branchRoot = branch_root;
            this.project = project;
            this.user = user;
            this.branch = branch;
            this.rebuild = rebuild;
            this.configuration = server.configuration;
            workPattern = Pattern.compile(server.configuration.getProgressRe());
        }

        private void substitute(String[] args) {
            String cwd = System.getProperty("user.dir");
            for (int i = 0; i < args.length; ++i) {
                args[i] = args[i].replace("{cwd}", cwd);
            }
        }

        private int execCommand(String working_dir, String[] command) throws IOException {
            this.stdOut = new StringBuffer();
            this.stdErr = new StringBuffer();

            ProcessBuilder pb = new ProcessBuilder(command);
            if (working_dir != null)
                pb.directory(new File(working_dir));

            Process process = pb.start();
            InputStream std = process.getInputStream();
            InputStream err = process.getErrorStream();

            int exitValue = 0;

            boolean done = false;
            while (!done) {
                if (cancel) {
                    process.destroy();
                    return 1;
                }

                try {
                    exitValue = process.exitValue();
                    done = true;
                } catch (IllegalThreadStateException e) {
                    try {
                        Thread.sleep(100);
                    } catch (InterruptedException e1) {
                        logger.error(e.getMessage(), e);
                    }
                }

                while (std.available() > 0) {
                    byte[] tmp = new byte[std.available()];
                    std.read(tmp);
                    stdOut.append(new String(tmp));
                }

                while (err.available() > 0) {
                    byte[] tmp = new byte[err.available()];
                    err.read(tmp);
                    stdErr.append(new String(tmp));
                    updateProgressStatus();
                }
            }

            std.close();
            err.close();
            try {
                exitValue = process.waitFor();
            } catch (InterruptedException e) {
                logger.error(e.getMessage(), e);
            }

            return exitValue;
        }

        public void cancel() {
            this.cancel = true;
        }

        @Override
        public void run() {
            try {
                doRun();
            } catch (Throwable e) {
                this.server.updateBuild(id, Activity.IDLE, BuildDesc.Result.ERROR, "", "Internal error");
                logger.error("Unexpected excetion caught", e);
            }
        }

        public void doRun() {
            String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
            try {
                String[] args = new String[configuration.getConfigureCommand().getArgsCount()];

                configuration.getConfigureCommand().getArgsList().toArray(args);
                substitute(args);

                int exitValue = execCommand(p, args);
                if (exitValue != 0) {
                    this.server.updateBuild(id, Activity.IDLE, BuildDesc.Result.ERROR, stdOut.toString(), stdErr.toString());
                    return;
                }

                if (rebuild) {
                    args = new String[configuration.getRebuildCommand().getArgsCount()];
                    configuration.getRebuildCommand().getArgsList().toArray(args);
                    substitute(args);
                } else {
                    args = new String[configuration.getBuildCommand().getArgsCount()];
                    configuration.getBuildCommand().getArgsList().toArray(args);
                    substitute(args);
                }
                exitValue = execCommand(p, args);
                if (exitValue != 0) {
                    this.server.updateBuild(id, Activity.IDLE, BuildDesc.Result.ERROR, stdOut.toString(), stdErr.toString());
                    return;
                }

                this.server.updateBuild(id, Activity.IDLE, BuildDesc.Result.OK, stdOut.toString(), stdErr.toString());
            } catch (IOException e) {
                logger.warn(e.getMessage(), e);
            }
        }

        public void updateProgressStatus() {
            Matcher m = workPattern.matcher(stdErr.toString());

            int progress = -1;
            int workAmount = -1;
            while (m.find()) {
                progress = Integer.valueOf(m.group(1));
                workAmount = Integer.valueOf(m.group(2));
            }

            if (progress != -1) {
                server.updateBuildProgress(id, progress, workAmount);
            }
        }
    }

    public BuildDesc build(String project, String user, String branch, boolean rebuild) throws ServerException {
        int build_number;
        synchronized(this) {

            for (Integer id : builds.keySet()) {
                RuntimeBuildDesc rtbd = builds.get(id);
                if (rtbd.project.equals(project) && rtbd.user.equals(user) && rtbd.branch.equals(branch) && rtbd.activity == Activity.BUILDING) {
                    // Build in progress
                    throw new ServerException("Build already in progress", Status.CONFLICT);
                }
            }

            build_number = nextBuildNumber++;
        }

        BuildDesc bd = BuildDesc.newBuilder()
            .setId(build_number)
            .setBuildActivity(Activity.BUILDING)
            .setBuildResult(BuildDesc.Result.OK)
            .setProgress(-1)
            .setWorkAmount(-1)
            .build();

        RuntimeBuildDesc rtbd = new RuntimeBuildDesc();
        rtbd.id = build_number;
        rtbd.project = project;
        rtbd.user = user;
        rtbd.branch = branch;

        rtbd.activity = Activity.BUILDING;
        rtbd.buildResult = BuildDesc.Result.OK;
        synchronized (builds) {
            builds.put(build_number, rtbd);
        }

        rtbd.buildRunnable = new BuildRunnable(this, build_number, branchRoot, project, user, branch, rebuild);
        rtbd.buildThread = new Thread(rtbd.buildRunnable);
        rtbd.buildThread.start();

        return bd;
    }

    public synchronized void updateBuild(int id, Activity activity, BuildDesc.Result result, String std_out, String std_err) {
        RuntimeBuildDesc rtbd;
        synchronized (builds) {
             rtbd = builds.get(id);
        }

        if (activity == Activity.BUILDING && rtbd.activity == Activity.IDLE) {
            throw new RuntimeException("Invalid transiation, IDLE to BUILDING");
        }

        rtbd.activity = activity;
        rtbd.buildResult = result;
        rtbd.stdOut = std_out;
        rtbd.stdErr = std_err;
        if (activity == Activity.IDLE) {
            rtbd.buildCompleted = System.currentTimeMillis();
        }
    }

    private synchronized void updateBuildProgress(int id, int progress, int workAmount) {
        RuntimeBuildDesc rtbd;
        synchronized (builds) {
            rtbd = builds.get(id);
        }
        rtbd.progress = progress;
        rtbd.workAmount = workAmount;
    }

    public BuildDesc buildStatus(String project, String user, String branch, int id) throws ServerException {
        RuntimeBuildDesc rtbd;
        synchronized (builds) {
            rtbd = builds.get(id);
        }

        if (rtbd == null) {
            throw new ServerException(String.format("Build %d not found", id), Status.NOT_FOUND);
        }

        BuildDesc bd = BuildDesc.newBuilder()
            .setId(rtbd.id)
            .setBuildActivity(rtbd.activity)
            .setBuildResult(rtbd.buildResult)
            .setProgress(rtbd.progress)
            .setWorkAmount(rtbd.workAmount)
            .build();

        return bd;
    }

    public void cancelBuild(String project, String user, String branch, int id) throws ServerException {
        RuntimeBuildDesc rtbd;
        synchronized (builds) {
            rtbd = builds.get(id);
        }

        if (rtbd == null) {
            throw new ServerException(String.format("Build %d not found", id), Status.NOT_FOUND);
        } else {
            rtbd.buildRunnable.cancel();
            try {
                rtbd.buildThread.join(5000);
            } catch (InterruptedException e) {
                logger.error("Unexpected interrupted exception", e);
            }
        }
    }

    public BuildLog buildLog(String project, String user, String branch, int id) {
        RuntimeBuildDesc rtbd;
        synchronized (builds) {
            rtbd = builds.get(id);
        }
        return BuildLog.newBuilder()
            .setStdOut(rtbd.stdOut)
            .setStdErr(rtbd.stdErr).build();
    }

    public LaunchInfo getLaunchInfo(EntityManager em, String project) throws ServerException {
        // We check that the project really exists here
        getProject(em, project);

        LaunchInfo.Builder b = LaunchInfo.newBuilder();
        for (String s : configuration.getExeCommand().getArgsList()) {
            b.addArgs(s);
        }

        return b.build();
    }

    public EntityManagerFactory getEntityManagerFactory() {
        return emf;
    }

    public Configuration getConfiguration() {
        return configuration;
    }


}
