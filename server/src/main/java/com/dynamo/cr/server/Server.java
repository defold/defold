package com.dynamo.cr.server;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.persistence.Query;
import javax.persistence.TypedQuery;
import javax.servlet.ServletContextEvent;
import javax.ws.rs.core.Response.Status;
import javax.ws.rs.core.UriBuilder;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.eclipse.jgit.api.Git;
import org.eclipse.jgit.api.errors.GitAPIException;
import org.eclipse.jgit.api.errors.NoHeadException;
import org.eclipse.jgit.http.server.GitServlet;
import org.eclipse.jgit.revwalk.RevCommit;
import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.glassfish.grizzly.PortRange;
import org.glassfish.grizzly.http.server.HttpHandler;
import org.glassfish.grizzly.http.server.HttpServer;
import org.glassfish.grizzly.http.server.NetworkListener;
import org.glassfish.grizzly.http.server.Response;
import org.glassfish.grizzly.http.server.ServerConfiguration;
import org.glassfish.grizzly.http.server.StaticHttpHandler;
import org.glassfish.grizzly.http.util.Header;
import org.glassfish.grizzly.servlet.ServletHandler;
import org.glassfish.grizzly.ssl.SSLContextConfigurator;
import org.glassfish.grizzly.ssl.SSLEngineConfigurator;
import org.joda.time.DateTime;
import org.joda.time.format.DateTimeFormat;
import org.joda.time.format.DateTimeFormatter;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.proto.Config.EMailTemplate;
import com.dynamo.cr.proto.Config.InvitationCountEntry;
import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.cr.server.auth.GitSecurityFilter;
import com.dynamo.cr.server.auth.OAuthAuthenticator;
import com.dynamo.cr.server.auth.SecurityFilter;
import com.dynamo.cr.server.mail.EMail;
import com.dynamo.cr.server.mail.IMailProcessor;
import com.dynamo.cr.server.model.Invitation;
import com.dynamo.cr.server.model.InvitationAccount;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.Prospect;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.resources.LoginOAuthResource;
import com.dynamo.cr.server.resources.LoginResource;
import com.dynamo.cr.server.resources.NewsListResource;
import com.dynamo.cr.server.resources.ProjectResource;
import com.dynamo.cr.server.resources.ProjectsResource;
import com.dynamo.cr.server.resources.ProspectsResource;
import com.dynamo.cr.server.resources.RepositoryResource;
import com.dynamo.cr.server.resources.ResourceUtil;
import com.dynamo.cr.server.resources.UsersResource;
import com.dynamo.inject.persist.PersistFilter;
import com.dynamo.inject.persist.jpa.JpaPersistModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.servlet.GuiceFilter;
import com.google.inject.servlet.GuiceServletContextListener;
import com.google.inject.servlet.ServletModule;
import com.sun.jersey.api.container.filter.RolesAllowedResourceFilterFactory;
import com.sun.jersey.guice.spi.container.servlet.GuiceContainer;
import com.sun.jersey.spi.container.servlet.ServletContainer;

public class Server {

    protected static Logger logger = LoggerFactory.getLogger(Server.class);

    private HttpServer httpServer;
    private String baseUri;
    private Configuration configuration;
    private EntityManagerFactory emf;
    private static final int MAX_ACTIVE_LOGINS = 1024;
    private SecureRandom secureRandom;

    private IMailProcessor mailProcessor;

    private ExecutorService executorService;

    // Value it retrieved from configuration in order to
    // be run-time changeable. Required for unit-tests
    private int openRegistrationMaxUsers;

    public static class Config extends GuiceServletContextListener {

        private Server server;

        public Config(Server server) {
            this.server = server;
        }

        @Override
        protected Injector getInjector() {
            return Guice.createInjector(new ServletModule(){
                @Override
                protected void configureServlets() {
                    Properties props = new Properties();
                    props.put(PersistenceUnitProperties.CLASSLOADER, server.getClass().getClassLoader());
                    install(new JpaPersistModule(server.getConfiguration().getPersistenceUnitName()).properties(props));

                    bind(Server.class).toInstance(server);
                    bind(OAuthAuthenticator.class).toInstance(new OAuthAuthenticator(MAX_ACTIVE_LOGINS));

                    bind(RepositoryResource.class);
                    bind(ProjectResource.class);
                    bind(ProjectsResource.class);
                    bind(UsersResource.class);
                    bind(NewsListResource.class);
                    bind(LoginResource.class);
                    bind(LoginOAuthResource.class);
                    bind(ProspectsResource.class);

                    Map<String, String> params = new HashMap<String, String>();
                    params.put("com.sun.jersey.config.property.resourceConfigClass",
                            "com.dynamo.cr.server.ResourceConfig");

                    params.put(ResourceConfig.PROPERTY_CONTAINER_REQUEST_FILTERS,
                            PreLoggingRequestFilter.class.getName() +  ";" + SecurityFilter.class.getName());
                    params.put(ResourceConfig.PROPERTY_RESOURCE_FILTER_FACTORIES,
                            RolesAllowedResourceFilterFactory.class.getName());
                    params.put(ResourceConfig.PROPERTY_CONTAINER_RESPONSE_FILTERS,
                            NoCacheHeaderResponseFilter.class.getName() + ";" +
                            CrossSiteHeaderResponseFilter.class.getName() + ";" +
                            PostLoggingResponseFilter.class.getName());

                    filter("*").through(PersistFilter.class);
                    serve("*").with(GuiceContainer.class, params);
                }
            });
        }
    }

    public class GuiceHandler extends ServletHandler {
        private final GuiceServletContextListener guiceServletContextListener;

        public GuiceHandler(Config guiceServletContextListener) {
            this.guiceServletContextListener = guiceServletContextListener;
        }

        @Override
        public void start() {
            guiceServletContextListener.contextInitialized(new ServletContextEvent(getServletCtx()));
            super.start();
        }

        @Override
        public void destroy() {
            super.destroy();
            guiceServletContextListener.contextDestroyed(new ServletContextEvent(getServletCtx()));
        }
    }

    HttpServer createHttpServer() throws IOException {
        // Manually create server to be able to tweak timeouts
        HttpServer server = new HttpServer();
        ServerConfiguration serverConfig = server.getServerConfiguration();
        serverConfig.addHttpHandler(new StaticHttpHandler("/"), "/");
        NetworkListener listener = new NetworkListener("grizzly", NetworkListener.DEFAULT_NETWORK_HOST, new PortRange(
                configuration.getServicePort()));
        if (configuration.hasGrizzlyIdleTimeout()) {
            listener.getKeepAlive().setIdleTimeoutInSeconds(configuration.getGrizzlyIdleTimeout());
        }
        server.addListener(listener);

        if (configuration.hasSslServicePort() && configuration.hasKeystore()) {
            SSLContextConfigurator sslCon = new SSLContextConfigurator();
            sslCon.setKeyStoreFile(configuration.getKeystore());
            String password = "defold";
            if (configuration.hasKeystorePassword()) {
                password = configuration.getKeystorePassword();
            }
            sslCon.setKeyStorePass(password);
            NetworkListener sslListener = new NetworkListener("ssl_grizzly", NetworkListener.DEFAULT_NETWORK_HOST,
                    new PortRange(configuration.getSslServicePort()));
            sslListener.setSecure(true);
            SSLEngineConfigurator ssle = new SSLEngineConfigurator(sslCon);
            sslListener.setSSLEngineConfig(ssle);
            ssle.setClientMode(false);

            if (configuration.hasGrizzlyIdleTimeout()) {
                sslListener.getKeepAlive().setIdleTimeoutInSeconds(configuration.getGrizzlyIdleTimeout());
            }
            server.addListener(sslListener);
        }

        Config config = new Config(this);
        ServletHandler handler = new GuiceHandler(config);
        handler.addFilter(new GuiceFilter(), "GuiceFilter", null);
        handler.setServletInstance(new ServletContainer());

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

        handler.addInitParameter("com.sun.jersey.config.property.resourceConfigClass",
                "com.dynamo.cr.server.ResourceConfig");

        handler.addInitParameter(ResourceConfig.PROPERTY_CONTAINER_REQUEST_FILTERS,
                PreLoggingRequestFilter.class.getName() +  ";" + SecurityFilter.class.getName());
        handler.addInitParameter(ResourceConfig.PROPERTY_RESOURCE_FILTER_FACTORIES,
                RolesAllowedResourceFilterFactory.class.getName());
        handler.addInitParameter(ResourceConfig.PROPERTY_CONTAINER_RESPONSE_FILTERS,
                NoCacheHeaderResponseFilter.class.getName() + ";" +
                CrossSiteHeaderResponseFilter.class.getName() + ";" +
                PostLoggingResponseFilter.class.getName());

        server.getServerConfiguration().addHttpHandler(handler, "/*");
        server.start();
        return server;
    }

    public Server() {}

    @Inject
    public Server(EntityManagerFactory emf,
                  Configuration configuration,
                  IMailProcessor mailProcessor) throws IOException {

        this.openRegistrationMaxUsers = configuration.getOpenRegistrationMaxUsers();

        this.emf = emf;
        this.configuration = configuration;
        this.mailProcessor = mailProcessor;

        try {
            secureRandom = SecureRandom.getInstance("SHA1PRNG");
        } catch (NoSuchAlgorithmException e1) {
            throw new RuntimeException(e1);
        }

        bootStrapUsers();

        baseUri = String.format("http://0.0.0.0:%d/", this.configuration.getServicePort());

        httpServer = createHttpServer();

        // TODO File caches are temporarily disabled to avoid two bugs:
        // * Content-type is incorrect for cached files:
        // http://java.net/jira/browse/GRIZZLY-1014
        // * Editor downloads consumes a lot of memory (and might leak)
        // Issue here: https://defold.fogbugz.com/default.asp?1177
        for (NetworkListener l : httpServer.getListeners()) {
            l.getFileCache().setEnabled(false);
            l.getFileCache().setMaxCacheEntries(0);
        }

        /*
         * NOTE: The GitServlet and friends seems to be a bit budget
         * If the servlet is mapped to http://host/git a folder "git"
         * must be present in the base-path, eg /tmp/git/... if the base-path
         * is /tmp
         */

        /*
         * NOTE: http.recivepack must be enabled in the server-side config, eg
         *
         * [http]
         *        receivepack = true
         *
         */

        String basePath = ResourceUtil.getGitBasePath(getConfiguration());
        logger.info("git base-path: {}", basePath);

        GitServlet gitServlet = new GitServlet();
        ServletHandler gitHandler = new ServletHandler(gitServlet);

        gitHandler.addFilter(new GitSecurityFilter(emf), "gitAuth", null);

        gitHandler.addInitParameter("base-path", basePath);
        gitHandler.addInitParameter("export-all", "1");

        String baseUri = ResourceUtil.getGitBaseUri(getConfiguration());
        logger.info("git base uri: {}", baseUri);
        httpServer.getServerConfiguration().addHttpHandler(gitHandler, baseUri);

        this.mailProcessor.start();

        this.executorService = Executors.newSingleThreadExecutor();
    }

    static class RedirectHandler extends HttpHandler {

        private String redirectHost;
        private int redirectPort;

        public RedirectHandler(String redirectHost,
                int redirectPort) {
            this.redirectHost = redirectHost;
            this.redirectPort = redirectPort;
        }

        @Override
        public void service(org.glassfish.grizzly.http.server.Request request,
                Response response) throws Exception {

            URI redirectUri = UriBuilder.fromUri(request.getRequestURL().toString())
                    .host(redirectHost)
                    .port(redirectPort)
                    .build();
            response.setHeader(Header.Location, redirectUri.toString());
            response.setStatus(org.glassfish.grizzly.http.util.HttpStatus.FOUND_302);
        }

    }

    public ExecutorService getExecutorService() {
        return executorService;
    }

    public IMailProcessor getMailProcessor() {
        return mailProcessor;
    }

    public SecureRandom getSecureRandom() {
        return secureRandom;
    }

    private void bootStrapUsers() {
        EntityManager em = emf.createEntityManager();
        em.getTransaction().begin();
        String[][] users = new String[][] {
                { "dynamogameengine@gmail.com", "admin", "Mr", "Admin" } };

        for (String[] entry : users) {
            User dynamoUser = ModelUtil.findUserByEmail(em, entry[0]);
            if (dynamoUser == null) {
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

    public void stop() {
        mailProcessor.stop();
        httpServer.stop();
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

    public InvitationAccount getInvitationAccount(EntityManager em, String userId) throws ServerException {
        InvitationAccount invitationAccount = em.find(InvitationAccount.class, Long.parseLong(userId));
        if (invitationAccount == null)
            throw new ServerException(String.format("No such invitation account %s", userId), javax.ws.rs.core.Response.Status.NOT_FOUND);

        return invitationAccount;
    }

    public int getInvitationCount(int originalCount) {
        for (InvitationCountEntry entry : this.configuration.getInvitationCountMapList()) {
            if (entry.getKey() == originalCount) {
                return entry.getValue();
            }
        }
        return 0;
    }

    public String getBaseURI() {
        return baseUri;
    }

    public Log log(EntityManager em, String project, int maxCount) throws IOException, ServerException, NoHeadException, GitAPIException {
        getProject(em, project);
        String sourcePath = String.format("%s/%s", configuration.getRepositoryRoot(), project);

        DateTimeFormatter formatter = DateTimeFormat.forPattern("yyyy-MM-dd HH:mm:ss Z");
        Log.Builder logBuilder = Log.newBuilder();
        Git git = Git.open(new File(sourcePath));
        Iterable<RevCommit> revLog = git.log().setMaxCount(maxCount).call();
        for (RevCommit revCommit : revLog) {
            CommitDesc.Builder commit = CommitDesc.newBuilder();
            commit.setName(revCommit.getCommitterIdent().getName());
            commit.setId(revCommit.getId().toString());
            commit.setMessage(revCommit.getShortMessage());
            commit.setEmail(revCommit.getCommitterIdent().getEmailAddress());
            long commitTime = revCommit.getCommitTime();
            commit.setDate(formatter.print(new DateTime(commitTime * 1000)));
            logBuilder.addCommits(commit);
        }

        return logBuilder.build();
    }

    public EntityManagerFactory getEntityManagerFactory() {
        return emf;
    }

    public Configuration getConfiguration() {
        return configuration;
    }

    public static String getEngineFilePath(Configuration configuration, String projectId, String platform) {
        return configuration.getSignedEngineRoot() + "/" + projectId + "/" + platform + "/" + "Defold.ipa";
    }

    public static File getEngineFile(Configuration configuration, String projectId, String platform) {
        return new File(Server.getEngineFilePath(configuration, projectId, platform));
    }

    public void uploadEngine(String projectId, String platform, InputStream stream) throws IOException {
        try {
            File outFile = getEngineFile(configuration, projectId, platform);
            outFile.getParentFile().mkdirs();
            FileUtils.copyInputStreamToFile(stream, outFile);
        } finally {
            IOUtils.closeQuietly(stream);
        }
    }

    /**
     * Calculate semi secure hash for signed executable
     * This is used for downloading without the requirement to be signed in
     * @param projectInfo
     * @return hash
     */
    public static String getEngineDownloadKey(Project project) {
        MessageDigest sha1;
        try {
            sha1 = MessageDigest.getInstance("SHA1");
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }

        String secret = "idfoiIIDIJFodfmd(/%(/jdd3232iifjg4545454";
        sha1.update(secret.getBytes());
        sha1.update(project.getDescription().getBytes());
        sha1.update(project.getCreated().toString().getBytes());
        sha1.update(Long.toString(project.getId()).getBytes());

        byte[] digest = sha1.digest();
        StringBuilder sb = new StringBuilder();
        for (byte b : digest) {
            sb.append(Integer.toHexString(0xff & b));
        }

        return sb.toString();
    }

    public byte[] downloadEngine(String projectId, String platform) throws IOException {
        File file = getEngineFile(configuration, projectId, platform);
        return FileUtils.readFileToByteArray(file);
    }

    public int getOpenRegistrationMaxUsers() {
        return openRegistrationMaxUsers;
    }

    public void setOpenRegistrationMaxUsers(int openRegistrationMaxUsers) {
        this.openRegistrationMaxUsers = openRegistrationMaxUsers;
    }

    public boolean openRegistration(EntityManager em) {
        Query query = em.createQuery("select count(u.id) from User u");
        Number count = (Number) query.getSingleResult();
        int maxUsers = getOpenRegistrationMaxUsers();
        boolean open = count.intValue() < maxUsers;
        return open;
    }

    public void invite(EntityManager em, String email, String inviter, String inviterEmail, int originalInvitationCount) {
        TypedQuery<Invitation> q = em.createQuery("select i from Invitation i where i.email = :email", Invitation.class);
        List<Invitation> lst = q.setParameter("email", email).getResultList();
        if (lst.size() > 0) {
            String msg = String.format("The email %s is already registred. An invitation will be sent to you as soon as we can " +
                                       "handle the high volume of registrations.", email);
            ResourceUtil.throwWebApplicationException(Status.CONFLICT, msg);
        }

        // Remove prospects
        Prospect p = ModelUtil.findProspectByEmail(em, email);
        if (p != null) {
            em.remove(p);
        }

        String key = UUID.randomUUID().toString();

        EMailTemplate template = getConfiguration().getInvitationTemplate();
        Map<String, String> params = new HashMap<String, String>();
        params.put("inviter", inviter);
        params.put("key", key);
        EMail emailMessage = EMail.format(template, email, params);

        Invitation invitation = new Invitation();
        invitation.setEmail(email);
        invitation.setInviterEmail(inviterEmail);
        invitation.setRegistrationKey(key);
        invitation.setInitialInvitationCount(getInvitationCount(originalInvitationCount));
        em.persist(invitation);
        getMailProcessor().send(em, emailMessage);
        em.flush();

        // NOTE: This is totally arbitrary. server.getMailProcessor().process()
        // should be invoked *after* the transaction is commited. Commits are
        // however container managed. Thats why we run a bit later.. budget..
        // The mail is eventually sent though as we periodically process the queue
        getExecutorService().execute(new Runnable() {
            @Override
            public void run() {
                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) { e.printStackTrace(); }
                getMailProcessor().process();
            }
        });

        ModelUtil.subscribeToNewsLetter(em, email, "", "");

    }


}
