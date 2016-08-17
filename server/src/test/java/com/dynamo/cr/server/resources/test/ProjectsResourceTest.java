package com.dynamo.cr.server.resources.test;

import com.dynamo.cr.protocol.proto.Protocol.*;
import com.dynamo.cr.server.auth.AccessTokenAuthenticator;
import com.dynamo.cr.server.auth.AccessTokenStore;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.providers.JsonProviders;
import com.dynamo.cr.server.providers.ProtobufProviders;
import com.google.common.io.Files;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.ClientResponse.Status;
import com.sun.jersey.api.client.UniformInterfaceException;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;
import com.sun.jersey.core.util.MultivaluedMapImpl;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ObjectNode;
import org.eclipse.jgit.api.Git;
import org.eclipse.jgit.api.errors.GitAPIException;
import org.eclipse.jgit.api.errors.TransportException;
import org.eclipse.jgit.lib.Ref;
import org.eclipse.jgit.transport.UsernamePasswordCredentialsProvider;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.rules.TestWatcher;
import org.junit.runner.Description;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import javax.persistence.TypedQuery;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.MultivaluedMap;
import javax.ws.rs.core.UriBuilder;
import javax.ws.rs.core.UriBuilderException;
import java.io.*;
import java.net.URI;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.*;

public class ProjectsResourceTest extends AbstractResourceTest {
    private static final Logger LOGGER = LoggerFactory.getLogger(ProjectsResourceTest.class);
    private static final int PORT = 6500;

    private String joeEmail = "joe@foo.com";
    private String joePasswd = "secret2";
    private String bobEmail = "bob@foo.com";
    private String bobPasswd = "secret3";
    private String adminEmail = "admin@foo.com";
    private String adminPasswd = "secret";
    private User joeUser;
    private User adminUser;
    private User bobUser;

    @SuppressWarnings("unused")
    private WebResource adminProjectsWebResource;
    private WebResource joeProjectsWebResource;

    private WebResource joeUsersWebResource;
    private WebResource bobProjectsWebResource;
    private WebResource bobUsersWebResource;

    private Client adminClient;
    private Client joeClient;
    private Client bobClient;

    private TestUser joe;
    private TestUser bob;

    static {
        // TODO: Workaround for a problem with double POST
        // testProjectInfo runs with success
        // testProjectInfoForbidden runs but with failing POST (using the *same* socket as testProjectInfo)
        // HttpURLconnection retries the POST on another socket => dead-lock occur.
        // Might be related to this issue:
        // Bug ID: 6427251 HttpURLConnection automatically retries non-idempotent method POST
        // http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=6427251
        System.setProperty("http.keepAlive", "false");
    }

    private static class TestUser {
        User user;
        String email;
        String password;
        WebResource projectsResource;

        TestUser(User user, String email, String password, WebResource projectsResource) {
            this.user = user;
            this.email = email;
            this.password = password;
            this.projectsResource = projectsResource;
        }
    }

    @Before
    public void setUp() throws Exception {
        setupUpTest();
        execCommand("scripts/setup_template_project.sh", "proj1");

        EntityManager em = emf.createEntityManager();
        em.getTransaction().begin();
        adminUser = new User();
        adminUser.setEmail(adminEmail);
        adminUser.setFirstName("undefined");
        adminUser.setLastName("undefined");
        adminUser.setPassword(adminPasswd);
        adminUser.setRole(Role.ADMIN);
        em.persist(adminUser);

        joeUser = new User();
        joeUser.setEmail(joeEmail);
        joeUser.setFirstName("undefined");
        joeUser.setLastName("undefined");
        joeUser.setPassword(joePasswd);
        joeUser.setRole(Role.USER);
        em.persist(joeUser);

        bobUser = new User();
        bobUser.setEmail(bobEmail);
        bobUser.setFirstName("undefined");
        bobUser.setLastName("undefined");
        bobUser.setPassword(bobPasswd);
        bobUser.setRole(Role.USER);
        em.persist(bobUser);

        em.getTransaction().commit();
        em.close();

        ClientConfig cc = new DefaultClientConfig();
        cc.getClasses().add(JsonProviders.ProtobufMessageBodyReader.class);
        cc.getClasses().add(JsonProviders.ProtobufMessageBodyWriter.class);
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        adminClient = Client.create(cc);
        adminClient.addFilter(new HTTPBasicAuthFilter(adminEmail, adminPasswd));

        URI uri = UriBuilder.fromUri("http://localhost/projects").port(PORT).build();
        adminProjectsWebResource = adminClient.resource(uri);

        joeClient = Client.create(cc);
        joeClient.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));

        uri = UriBuilder.fromUri("http://localhost/projects").port(PORT).build();
        joeProjectsWebResource = joeClient.resource(uri);

        uri = UriBuilder.fromUri("http://localhost/users").port(PORT).build();
        joeUsersWebResource = joeClient.resource(uri);

        bobClient = Client.create(cc);
        bobClient.addFilter(new HTTPBasicAuthFilter(bobEmail, bobPasswd));

        uri = UriBuilder.fromUri("http://localhost/projects").port(PORT).build();
        bobProjectsWebResource = bobClient.resource(uri);

        uri = UriBuilder.fromUri("http://localhost/users").port(PORT).build();
        bobUsersWebResource = bobClient.resource(uri);

        joe = new TestUser(joeUser, joeEmail, joePasswd, joeProjectsWebResource);
        bob = new TestUser(bobUser, bobEmail, bobPasswd, bobProjectsWebResource);
    }

    @After
    public void tearDown() throws Exception {
        adminClient.destroy();
        joeClient.destroy();
        bobClient.destroy();
    }

    @Rule
    public TestRule watcher = new TestWatcher() {
        protected void starting(Description description) {
            LOGGER.info("Starting test: " + description.getMethodName());
        }

        @Override
        protected void finished(Description description) {
            LOGGER.info("Finished test: " + description.getMethodName());
        }
    };

    /**
     * The sole purpose of this test is too boot everything up, since the actual test below sometimes fail on the server
     * due to time out exceptions.
     *
     * @see https://defold.fogbugz.com/default.asp?2376
     * @throws Exception
     */
    @Test
    public void testFakeBootup() throws Exception {
        NewProject newProject = NewProject.newBuilder().setName("test project").setDescription("New test project")
                .build();

        try {
            @SuppressWarnings("unused")
            ProjectInfo projectInfo = joeProjectsWebResource.path(joeUser.getId().toString())
                    .accept(ProtobufProviders.APPLICATION_XPROTOBUF).type(ProtobufProviders.APPLICATION_XPROTOBUF)
                    .post(ProjectInfo.class, newProject);
        } catch (Throwable t) {
            // silent pass
        }
    }

    @Test
    public void testProjectInfo() throws Exception {
        NewProject newProject = NewProject.newBuilder().setName("test project").setDescription("New test project")
                .build();

        ProjectInfo projectInfo = joeProjectsWebResource
            .path(joeUser.getId().toString())
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .type(ProtobufProviders.APPLICATION_XPROTOBUF)
            .post(ProjectInfo.class, newProject);

        ClientResponse response;
        response = joeProjectsWebResource.path(String.format("/%d/%d/project_info", joeUser.getId(), projectInfo.getId())).get(ClientResponse.class);
        assertEquals(200, response.getStatus());
        response.getEntity(ProjectInfo.class);
    }

    private class StressRunnable implements Runnable {
        private long projectId;
        public Throwable exception;

        StressRunnable(long projectId) {
            this.projectId = projectId;
        }

        @Override
        public void run() {
            try {
                for (int i = 0; i < 1000; ++i) {
                    ClientResponse response;
                    response = joeProjectsWebResource.path(String.format("/%d/%d/project_info", joeUser.getId(), projectId)).get(ClientResponse.class);
                    assertEquals(200, response.getStatus());
                    response.getEntity(ProjectInfo.class);

                    @SuppressWarnings("unused")
                    UserInfoList joesConnections = joeUsersWebResource
                        .path(String.format("/%d/connections", joeUser.getId()))
                        .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                        .get(UserInfoList.class);
                }
            } catch (Throwable e) {
                this.exception = e;
            }
        }

    }

    @Test
    public void testStress() throws Exception {
        NewProject newProject = NewProject.newBuilder()
                .setName("test project")
                .setDescription("New test project").build();

        final ProjectInfo projectInfo = joeProjectsWebResource
            .path(joeUser.getId().toString())
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .type(ProtobufProviders.APPLICATION_XPROTOBUF)
            .post(ProjectInfo.class, newProject);



        StressRunnable runnable1 = new StressRunnable(projectInfo.getId());
        StressRunnable runnable2 = new StressRunnable(projectInfo.getId());

        Thread t1 = new Thread(runnable1);
        Thread t2 = new Thread(runnable2);
        t1.start();
        t2.start();
        t1.join(100000);
        t2.join(100000);

        assertEquals(null, runnable1.exception);
        assertEquals(null, runnable2.exception);
    }

    @Test
    public void testProjectInfoForbidden() throws Exception {
        NewProject newProject = NewProject.newBuilder()
                .setName("test project123456789")
                .setDescription("New test project").build();

        ProjectInfo projectInfo = joeProjectsWebResource
            .path(joeUser.getId().toString())
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .type(ProtobufProviders.APPLICATION_XPROTOBUF)
            .post(ProjectInfo.class, newProject);

        ClientResponse response;
        response = bobProjectsWebResource.path(String.format("/%d/%d/project_info", bobUser.getId(), projectInfo.getId())).get(ClientResponse.class);
        assertEquals(403, response.getStatus());
    }

    @Test
    public void newProject() throws Exception {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        TypedQuery<Project> query = em.createQuery("select p from Project p", Project.class);
        int nprojects = query.getResultList().size();

        NewProject newProject = NewProject.newBuilder()
                .setName("test project")
                .setDescription("New test project").build();

        ProjectInfo projectInfo = joeProjectsWebResource
            .path(joeUser.getId().toString())
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .type(ProtobufProviders.APPLICATION_XPROTOBUF)
            .post(ProjectInfo.class, newProject);

        // Add bob as member
        joeProjectsWebResource.path(String.format("/%d/%d/members", joeUser.getId(), projectInfo.getId()))
            .post(bobEmail);

        UserInfoList joesConnections = joeUsersWebResource
                                        .path(String.format("/%d/connections", joeUser.getId()))
                                        .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                                        .get(UserInfoList.class);

        // Check that joe now is connected to bob
        assertEquals(1, joesConnections.getUsersCount());
        assertEquals(bobEmail, joesConnections.getUsers(0).getEmail());

        UserInfoList bobsConnections = bobUsersWebResource
                    .path(String.format("/%d/connections", bobUser.getId()))
                    .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                    .get(UserInfoList.class);

        // Check that bob is still not connected to joe
        assertEquals(0, bobsConnections.getUsersCount());

        assertEquals("test project", projectInfo.getName());
        assertEquals(joeUser.getId().longValue(), projectInfo.getOwner().getId());
        assertEquals(joeUser.getEmail(), projectInfo.getOwner().getEmail());

        // Check that bob can retrieve project info
        ProjectInfo bobsProjectInfo = bobProjectsWebResource
            .path(String.format("%d/%d/project_info", bobUser.getId(), projectInfo.getId()))
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .type(ProtobufProviders.APPLICATION_XPROTOBUF)
            .get(ProjectInfo.class);
        assertEquals(bobsProjectInfo.getId(), projectInfo.getId());

        ProjectInfoList list = joeProjectsWebResource
            .path(joeUser.getId().toString())
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .get(ProjectInfoList.class);

        assertEquals(joeUser.getId().longValue(), list.getProjects(0).getOwner().getId());
        assertEquals(nprojects + 1, query.getResultList().size());
        assertEquals(2, list.getProjects(0).getMembersCount());

        // Remove joe as bob as member
        ClientResponse response = bobProjectsWebResource
            .path(String.format("/%d/%d/members/%d", bobUser.getId(), projectInfo.getId(), joeUser.getId()))
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .delete(ClientResponse.class);

        assertEquals(ClientResponse.Status.FORBIDDEN.getStatusCode(), response.getStatus());

        // Remove bob as member
        joeProjectsWebResource
            .path(String.format("/%d/%d/members/%d", joeUser.getId(), projectInfo.getId(), bobUser.getId()))
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .delete();

        list = joeProjectsWebResource
            .path(joeUser.getId().toString())
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .get(ProjectInfoList.class);

        assertEquals(1, list.getProjects(0).getMembersCount());

        File gitObjectDir = new File(String.format("tmp/test_data/%d/objects", projectInfo.getId()));
        assertTrue(gitObjectDir.isDirectory());
        assertTrue(gitObjectDir.exists());
        em.close();
    }

    @Test
    public void newProjectFromTemplate() throws Exception {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        TypedQuery<Project> query = em.createQuery("select p from Project p", Project.class);
        int nprojects = query.getResultList().size();

        ObjectMapper m = new ObjectMapper();
        ObjectNode project = m.createObjectNode();
        project.put("name", "test project");
        project.put("description", "New test project");
        project.put("templateId", "proj1");

        ProjectInfo projectInfo = joeProjectsWebResource
            .path(joeUser.getId().toString())
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .post(ProjectInfo.class, project.toString());

        assertEquals("test project", projectInfo.getName());
        assertEquals(joeUser.getId().longValue(), projectInfo.getOwner().getId());
        assertEquals(joeUser.getEmail(), projectInfo.getOwner().getEmail());

        ProjectInfoList list = joeProjectsWebResource
            .path(joeUser.getId().toString())
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .get(ProjectInfoList.class);

        assertEquals(1, list.getProjectsList().size());
        assertEquals(joeUser.getId().longValue(), list.getProjects(0).getOwner().getId());

        assertEquals(nprojects + 1, query.getResultList().size());

        File gitObjectDir = new File(String.format("tmp/test_data/%d/objects", projectInfo.getId()));
        assertTrue(gitObjectDir.isDirectory());
        assertTrue(gitObjectDir.exists());

        em.close();
    }

    @Test
    public void newProjectFromMissingTemplate() throws Exception {

        ObjectMapper m = new ObjectMapper();
        ObjectNode project = m.createObjectNode();
        project.put("name", "test project");
        project.put("description", "New test project");
        project.put("templateId", "projX");

        ClientResponse clientResponse = joeProjectsWebResource
                .path(joeUser.getId().toString())
                .accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE)
                .post(ClientResponse.class, project.toString());

        assertEquals(500, clientResponse.getStatus());

        ProjectInfoList list = joeProjectsWebResource
                .path(joeUser.getId().toString())
                .accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE)
                .get(ProjectInfoList.class);

        assertEquals(0, list.getProjectsList().size());
    }

    private static ProjectInfo createTemplateProject(TestUser testUser, String templateId) {
        ObjectMapper m = new ObjectMapper();
        ObjectNode project = m.createObjectNode();
        project.put("name", "test project");
        project.put("description", "New test project");
        project.put("templateId", templateId);

        ProjectInfo projectInfo = testUser.projectsResource
            .path(testUser.user.getId().toString())
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .post(ProjectInfo.class, project.toString());

        assertEquals("test project", projectInfo.getName());
        return projectInfo;
    }

    private static String cloneRepoHttpAuth(TestUser testUser, ProjectInfo projectInfo) throws IOException, GitAPIException {
        File cloneDir = Files.createTempDir();
        UsernamePasswordCredentialsProvider provider = new UsernamePasswordCredentialsProvider(testUser.email, testUser.password);
        Git.cloneRepository()
            .setCredentialsProvider(provider)
            .setURI(projectInfo.getRepositoryUrl())
            .setDirectory(cloneDir)
            .call();

        return cloneDir.getAbsolutePath();
    }

    private String cloneRepoOpenID(TestUser testUser, ProjectInfo projectInfo) throws IOException, GitAPIException {
        File cloneDir = Files.createTempDir();
        UsernamePasswordCredentialsProvider provider = new UsernamePasswordCredentialsProvider(testUser.email, testUser.password);
        Git.cloneRepository()
            .setCredentialsProvider(provider)
            .setURI(projectInfo.getRepositoryUrl())
            .setDirectory(cloneDir)
            .call();

        return cloneDir.getAbsolutePath();
    }

    @Test
    public void cloneHTTPBasicAuth() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        cloneRepoHttpAuth(joe, projectInfo);
    }

    @Test
    public void cloneOpenID() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        cloneRepoOpenID(joe, projectInfo);
    }

    @Test(expected = TransportException.class)
    public void cloneHTTPBasicAuthAccessDenied() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        cloneRepoHttpAuth(bob, projectInfo);
    }

    @Test(expected = TransportException.class)
    public void cloneOpenIDAccessDenied() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        cloneRepoOpenID(bob, projectInfo);
    }

    private ClientResponse testGetArchive(String version, String sha1) throws IOException {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        return testGetArchive(version, sha1, projectInfo);
    }

    private ClientResponse testGetArchive(String version, String sha1, ProjectInfo projectInfo) throws IOException {
        String path = String.format("/%d/%d/archive/%s", -1, projectInfo.getId(), version);
        WebResource resource = joeProjectsWebResource.path(path);
        if (sha1 != null) {
            return resource.header("If-None-Match", sha1).get(ClientResponse.class);
        } else {
            return resource.get(ClientResponse.class);

        }
    }

    private void verifyArchive(ClientResponse response) throws IOException {
        InputStream is = response.getEntityInputStream();
        ByteArrayOutputStream os = new ByteArrayOutputStream();
        IOUtils.copy(is, os);
        IOUtils.closeQuietly(is);
        File zip = File.createTempFile("tmp", "zip");
        zip.deleteOnExit();
        FileUtils.writeByteArrayToFile(zip, os.toByteArray());

        String etag = response.getHeaders().getFirst("ETag");
        ZipFile zipFile = new ZipFile(zip);
        assertEquals(etag, zipFile.getComment());
        Enumeration<? extends ZipEntry> entries = zipFile.entries();
        Set<String> names = new HashSet<>();
        while (entries.hasMoreElements()) {
            ZipEntry ze = entries.nextElement();
            names.add(ze.getName());
        }
        zipFile.close();

        assertEquals(new HashSet<>(Arrays.asList("content/test space.txt", "content/file1.txt", "content/file2.txt")), names);
    }

    // NOTE:
    // Tests getArchiveX should more accurately be in ProjectResourceTest.java
    // but current test, including helper functions for creating projects, resides in ProjectsResourceTest.java
    @Test
    public void getArchive1() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        ClientResponse response = testGetArchive("", null, projectInfo);
        String sha1 = response.getHeaders().getFirst("ETag");
        assertEquals(200, response.getStatus());
        verifyArchive(response);
        response = testGetArchive("", sha1, projectInfo);
        assertEquals(304, response.getStatus());
    }

    @Test
    public void getArchive2() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        ClientResponse response = testGetArchive("master", null, projectInfo);
        String sha1 = response.getHeaders().getFirst("ETag");
        assertEquals(200, response.getStatus());
        verifyArchive(response);
        response = testGetArchive("", sha1, projectInfo);
        assertEquals(304, response.getStatus());
    }

    @Test
    public void getArchive3() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        ClientResponse response = testGetArchive("HEAD", null, projectInfo);
        String sha1 = response.getHeaders().getFirst("ETag");
        assertEquals(200, response.getStatus());
        verifyArchive(response);
        response = testGetArchive("", sha1, projectInfo);
        assertEquals(304, response.getStatus());
    }

    @Test
    public void getArchive4() throws Exception {
        ClientResponse response = testGetArchive("INVALID", null);
        String sha1 = response.getHeaders().getFirst("ETag");
        assertEquals(null, sha1);
        assertEquals(404, response.getStatus());
    }

    @Test
    public void getCachedArchive() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        ClientResponse response1 = testGetArchive("", null, projectInfo);
        String sha1 = response1.getHeaders().getFirst("ETag");
        assertEquals(200, response1.getStatus());
        verifyArchive(response1);

        ClientResponse response2 = testGetArchive("", null, projectInfo);
        String sha2 = response2.getHeaders().getFirst("ETag");
        assertEquals(200, response2.getStatus());
        assertEquals(sha1, sha2);
        verifyArchive(response2);
    }

    @Test
    public void getArchiveWithCacheMiss() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        ClientResponse response1 = testGetArchive("", null, projectInfo);
        String sha1 = response1.getHeaders().getFirst("ETag");
        assertEquals(200, response1.getStatus());
        verifyArchive(response1);

        clearTemporaryFileArea();

        ClientResponse response2 = testGetArchive("", null, projectInfo);
        String sha2 = response2.getHeaders().getFirst("ETag");
        assertEquals(200, response2.getStatus());
        assertEquals(sha1, sha2);
        verifyArchive(response2);
    }


    private class ArchiveStressRunnable implements Runnable {
        private long projectId;
        public Throwable exception;

        ArchiveStressRunnable(long projectId) {
            this.projectId = projectId;
        }

        @Override
        public void run() {
            try {
                for(int i=0; i < 1000; ++i) {
                    String path = String.format("/%d/%d/archive/", -1, projectId);
                    WebResource resource = joeProjectsWebResource.path(path);

                    ClientResponse response = resource.get(ClientResponse.class);
                    assertEquals(200, response.getStatus());
                    verifyArchive(response);
                }
            } catch (Throwable e) {
                this.exception = e;
            }
        }
    }

    @Test
    public void archiveStressTest() throws Exception {
        final ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        ArchiveStressRunnable runnable1 = new ArchiveStressRunnable(projectInfo.getId());
        ArchiveStressRunnable runnable2 = new ArchiveStressRunnable(projectInfo.getId());

        Thread t1 = new Thread(runnable1);
        Thread t2 = new Thread(runnable2);
        t1.start();
        t2.start();
        t1.join(100000);
        t2.join(100000);

        assertEquals(null, runnable1.exception);
        assertEquals(null, runnable2.exception);
    }


    private ClientResponse getArchiveETag(String version) {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        return getArchiveETag(version, projectInfo.getId());
    }

    private ClientResponse getArchiveETag(String version, long projectId) {
        String path = String.format("/%d/%d/archive/%s", -1, projectId, version);
        return joeProjectsWebResource.path(path).head();
    }

    @Test
    public void getArchiveHead() throws IOException, GitAPIException {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        String head = getArchiveETag("HEAD", projectInfo.getId()).getHeaders().getFirst("ETag");

        // Add new tag on latest commit
        // Note: relies on internal server implementation for project storage
        File projectPath = getServerProjectPath(projectInfo.getId());
        Git git = Git.open(projectPath);
        git.tag().setName("1.0.0").call();

        String _1_0 = getArchiveETag("1.0.0", projectInfo.getId()).getHeaders().getFirst("ETag");
        assertEquals(head, _1_0);

        ClientResponse foo = getArchiveETag("foo");
        assertEquals(Status.NOT_FOUND.getStatusCode(), foo.getStatus());
    }

    @Test
    public void getArchiveByHash() {
        String head1 = getArchiveETag("HEAD").getHeaders().getFirst("ETag");
        String head2 = getArchiveETag(head1).getHeaders().getFirst("ETag");
        assertEquals(head1, head2);
    }

    @Test
    public void getArchiveByBranch() throws IOException, GitAPIException {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");

        // Add new branch on latest commit
        // Note: relies on internal server implementation for project storage
        File projectPath = getServerProjectPath(projectInfo.getId());
        Git git = Git.open(projectPath);
        Ref branchRef = git.branchCreate().setName("testbranch").call();
        String branchSha = branchRef.getObjectId().getName();

        String branch2 = getArchiveETag("testbranch", projectInfo.getId()).getHeaders().getFirst("ETag");
        assertEquals(branchSha, branch2);
        String branch_fail = getArchiveETag("non-existing-branch", projectInfo.getId()).getHeaders().getFirst("ETag");
        assertEquals(null, branch_fail);
    }

    private static File getServerProjectPath(long projectId) {
        String repositoryRoot = server.getConfiguration().getRepositoryRoot();
        return new File(String.format("%s/%d", repositoryRoot, projectId));
    }

    @Test
    public void testLog() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        int maxCount = 5;

        MultivaluedMap<String, String> queryParams = new MultivaluedMapImpl();
        queryParams.add("max_count", String.valueOf(maxCount));

        Log projectLog = joeProjectsWebResource
                .path(String.format("%d/%d/log", -1, projectInfo.getId()))
                .queryParams(queryParams)
                .get(Log.class);

        int originalCommitsCount = projectLog.getCommitsCount();

        File tempDir = Files.createTempDir();
        try {
            File repositoryDir = getRepositoryDir(projectInfo.getId());
            Git git = Git.cloneRepository()
                    .setURI(repositoryDir.getAbsolutePath())
                    .setDirectory(tempDir)
                    .call();

            File contentDir = new File(tempDir.getAbsolutePath() + "/content");
            String tempFilename = "testfile.tmp";
            File newFile = new File(contentDir, tempFilename);
            newFile.createNewFile();

            git.add().addFilepattern(tempFilename).call();
            git.commit().setMessage("Added " + tempFilename).call();
            git.push().call();

            git.close();
        } finally {
            FileUtils.deleteQuietly(tempDir);
        }

        int commitsCount = joeProjectsWebResource
                .path(String.format("%d/%d/log", -1, projectInfo.getId()))
                .queryParams(queryParams)
                .get(Log.class)
                .getCommitsCount();

        assertTrue(commitsCount == originalCommitsCount + 1);
    }

    private static void alterFile(String cloneDir, String name, String content) throws IOException {
        File file = new File(cloneDir + "/" + name);
        assertTrue(file.exists());
        FileOutputStream fos = new FileOutputStream(file);
        fos.write(content.getBytes());
        fos.close();
    }

    @Test
    public void pushHTTPBasicAuth() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        String cloneDir = cloneRepoHttpAuth(joe, projectInfo);
        alterFile(cloneDir, "content/file1.txt", "some content");

        UsernamePasswordCredentialsProvider provider = new UsernamePasswordCredentialsProvider(joeEmail, joePasswd);
        Git git = Git.open(new File(cloneDir));
        git.commit().setAll(true).setMessage("a commit").call();
        git.push().setCredentialsProvider(provider).call();
    }

    @Test
    public void pushOpenID() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        String cloneDir = cloneRepoHttpAuth(joe, projectInfo);
        alterFile(cloneDir, "content/file1.txt", "some content");

        UsernamePasswordCredentialsProvider provider = new UsernamePasswordCredentialsProvider(joeEmail, joePasswd);
        Git git = Git.open(new File(cloneDir));
        git.commit().setAll(true).setMessage("a commit").call();
        git.push().setCredentialsProvider(provider).call();
    }

    @Test
    public void newProjectFromInvalidTemplateId() throws Exception {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        TypedQuery<Project> query = em.createQuery("select p from Project p", Project.class);
        int nprojects = query.getResultList().size();

        ObjectMapper m = new ObjectMapper();
        ObjectNode project = m.createObjectNode();
        project.put("name", "test project");
        project.put("description", "New test project");
        project.put("templateId", "does_not_exists");

        ClientResponse response = joeProjectsWebResource
            .path(joeUser.getId().toString())
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .post(ClientResponse.class, project.toString());

        assertEquals(500, response.getStatus());
        // Make sure that no project is left in the database
        assertEquals(nprojects, query.getResultList().size());
        em.close();
    }

    @Test
    public void testListProjects() throws Exception {
        NewProject newProject = NewProject.newBuilder()
                .setName("test project123456789")
                .setDescription("New test project").build();

        joeProjectsWebResource
            .path(joeUser.getId().toString())
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .type(ProtobufProviders.APPLICATION_XPROTOBUF)
            .post(ProjectInfo.class, newProject);

        ClientResponse response;
        ProjectInfoList list;

        response = joeProjectsWebResource.path(String.format("/%d", joeUser.getId())).get(ClientResponse.class);
        assertEquals(200, response.getStatus());
        list = response.getEntity(ProjectInfoList.class);
        assertEquals(1, list.getProjectsCount());
        assertEquals(joeUser.getEmail(), list.getProjects(0).getOwner().getEmail());

        response = bobProjectsWebResource.path(String.format("/%d", bobUser.getId())).get(ClientResponse.class);
        assertEquals(200, response.getStatus());
        list = response.getEntity(ProjectInfoList.class);
        assertEquals(0, list.getProjectsCount());

        response = bobProjectsWebResource.path(String.format("/%d", joeUser.getId())).get(ClientResponse.class);
        assertEquals(200, response.getStatus());
        list = response.getEntity(ProjectInfoList.class);
        assertEquals(0, list.getProjectsCount());
    }

    @Test
    public void testRunGitGc() throws Exception {

        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        long projectId = projectInfo.getId();

        WebResource gitWebResource = getJoeGitWebResource();

        File repositoryDir = getRepositoryDir(projectId);
        File refFile = new File(repositoryDir.getCanonicalPath() + File.separator + "packed-refs");
        gitWebResource.path("" + projectId + "/info/refs").queryParam("service", "git-receive-pack").post();

        Thread.sleep(200); // Wait for garbage collect to complete
        assertTrue(refFile.exists());
    }

    @Test
    public void testRemoveGcLockFile() throws Exception {

        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        long projectId = projectInfo.getId();

        WebResource gitWebResource = getJoeGitWebResource();
        File repositoryDir = getRepositoryDir(projectId);

        File refFile = new File(repositoryDir.getCanonicalPath() + File.separator + "packed-refs");
        File lockFile = new File(repositoryDir.getCanonicalPath() + File.separator + "packed-refs.lock");
        writePackedRefsFile(refFile, false);
        writePackedRefsFile(lockFile, true);

        gitWebResource.path("" + projectId + "/info/refs").queryParam("service", "git-receive-pack").post();
        Thread.sleep(200); // Wait for garbage collect to remove the lockfile
        assertFalse(lockFile.exists());
    }

    private WebResource getJoeGitWebResource() throws IllegalArgumentException, UriBuilderException {
        URI uri = UriBuilder.fromUri("http://localhost/test_data").port(PORT).build();
        ClientConfig cc = new DefaultClientConfig();
        Client gitClient = Client.create(cc);

        gitClient.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        return gitClient.resource(uri);
    }

    private void writePackedRefsFile(File file, boolean isLockFile) throws IOException {
        String newline = System.getProperty("line.separator");
        try (BufferedWriter writer = new BufferedWriter(new FileWriter(file))) {
            writer.write("# pack-refs with: peeled" + newline);
            if(isLockFile) {
                writer.write("7f7b9aff9fd418684d7d414cf14862ac52ea676c refs/heads/master" + newline);
            } else {
                writer.write("12bc56a629faa909c21c8e1fd1a63f7305e1923b refs/heads/master" + newline);
            }
            writer.write("27c7d125e6d93b6783abafa5dcdc2eafcc21b2f2 refs/heads/testbranch" + newline);
            writer.write("cd76e8f5a5014b1f39836349688f0a806de8c692 refs/tags/1.0" + newline);
            writer.write("^27c7d125e6d93b6783abafa5dcdc2eafcc21b2f2" + newline);
        }
    }

    private class GitPushRunnable implements Runnable {
        private long projectId;
        public Throwable exception;

        GitPushRunnable(long projectId) {
            this.projectId = projectId;
        }

        @Override
        public void run() {
            File tempDir = Files.createTempDir();
            try {
                File repositoryDir = getRepositoryDir(projectId);
                Git git = Git.cloneRepository()
                        .setURI(repositoryDir.getAbsolutePath())
                        .setDirectory(tempDir)
                        .call();

                File contentDir = new File(tempDir.getAbsolutePath() + "/content");

                for(int i=0; i<60; i++) {
                    String tempFilename = String.format("testfile_%d.tmp", i);
                    File newFile = new File(contentDir, tempFilename);
                    newFile.createNewFile();

                    git.add().addFilepattern(tempFilename).call();
                    git.commit().setMessage("Added " + tempFilename).call();
                    git.push().call();
                }
                git.close();
            } catch (Throwable e) {
                this.exception = e;
            } finally {
                FileUtils.deleteQuietly(tempDir);
            }
        }
    }

    @Test
    public void testGitGcWithPush() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        long projectId = projectInfo.getId();

        GitPushRunnable runnable1 = new GitPushRunnable(projectId);

        Thread t1 = new Thread(runnable1);
        t1.start();
        // Trigger 10 garbage collects while git push is running
        for(int i=0; i<10; i++) {
            WebResource gitWebResource = getJoeGitWebResource();
            gitWebResource.path("" + projectId + "/info/refs").queryParam("service", "git-receive-pack").post();
        }
        t1.join(15000);
        Thread.sleep(200); // wait for the garbage collect to finish

        assertEquals(null, runnable1.exception);
    }

}
