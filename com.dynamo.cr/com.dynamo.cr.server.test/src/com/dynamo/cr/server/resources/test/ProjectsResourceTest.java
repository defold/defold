package com.dynamo.cr.server.resources.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.URI;

import javax.persistence.EntityManager;
import javax.persistence.TypedQuery;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.UriBuilder;

import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ObjectNode;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.rules.TestWatcher;
import org.junit.runner.Description;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.common.providers.JsonProviders;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.protocol.proto.Protocol.NewProject;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfoList;
import com.dynamo.cr.protocol.proto.Protocol.ProjectStatus;
import com.dynamo.cr.protocol.proto.Protocol.UserInfoList;
import com.dynamo.cr.server.auth.AuthCookie;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.server.dgit.CommandUtil;
import com.dynamo.server.dgit.GitException;
import com.dynamo.server.dgit.GitFactory;
import com.dynamo.server.dgit.IGit;
import com.google.common.io.Files;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.UniformInterfaceException;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;


public class ProjectsResourceTest extends AbstractResourceTest {
    private static class TestUser {
        User user;
        String email;
        String password;
        WebResource projectsResource;

        public TestUser(User user, String email, String password, WebResource projectsResource) {
            this.user = user;
            this.email = email;
            this.password = password;
            this.projectsResource = projectsResource;
        }
    }

    protected static Logger logger = LoggerFactory.getLogger(ProjectsResourceTest.class);

    static int port = 6500;
    String joeEmail = "joe@foo.com";
    String joePasswd = "secret2";
    String bobEmail = "bob@foo.com";
    String bobPasswd = "secret3";
    String adminEmail = "admin@foo.com";
    String adminPasswd = "secret";
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

    void execCommand(String command, String arg) throws IOException {
        CommandUtil.Result r = CommandUtil.execCommand(new String[] {"/bin/bash", command, arg});
        if (r.exitValue != 0) {
            System.err.println(r.stdOut);
            System.err.println(r.stdErr);
        }
        assertEquals(0, r.exitValue);
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

        URI uri = UriBuilder.fromUri(String.format("http://localhost/projects")).port(port).build();
        adminProjectsWebResource = adminClient.resource(uri);

        joeClient = Client.create(cc);
        joeClient.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));

        uri = UriBuilder.fromUri(String.format("http://localhost/projects")).port(port).build();
        joeProjectsWebResource = joeClient.resource(uri);

        uri = UriBuilder.fromUri(String.format("http://localhost/users")).port(port).build();
        joeUsersWebResource = joeClient.resource(uri);

        bobClient = Client.create(cc);
        bobClient.addFilter(new HTTPBasicAuthFilter(bobEmail, bobPasswd));

        uri = UriBuilder.fromUri(String.format("http://localhost/projects")).port(port).build();
        bobProjectsWebResource = bobClient.resource(uri);

        uri = UriBuilder.fromUri(String.format("http://localhost/users")).port(port).build();
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
            logger.info("Starting test: " + description.getMethodName());
        }

        @Override
        protected void finished(Description description) {
            logger.info("Finished test: " + description.getMethodName());
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

    class StressRunnable implements Runnable {

        private long projectId;
        public Throwable exception;

        public StressRunnable(long projectId) {
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
    public void newProjectCap() throws Exception {
        /*
         * Bob creates one project
         * Bob adds joe as member to ensure that we count only projects that the user own
         * Joe creates maxProjectCount projects
         * Joe creates one additional project that should fail
         */
        int maxProjectCount = server.getConfiguration().getMaxProjectCount();

        NewProject newProject = NewProject.newBuilder()
                .setName("bob's test project")
                .setDescription("New test project").build();
        ProjectInfo projectInfo = bobProjectsWebResource
                .path(bobUser.getId().toString())
                .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                .type(ProtobufProviders.APPLICATION_XPROTOBUF)
                .post(ProjectInfo.class, newProject);

        // Add joe as member
        bobProjectsWebResource.path(String.format("/%d/%d/members", bobUser.getId(), projectInfo.getId())).post(joeEmail);

        for (int i = 0; i < maxProjectCount; ++i) {
            newProject = NewProject.newBuilder()
                    .setName("test project" + i)
                    .setDescription("New test project").build();
            joeProjectsWebResource
                    .path(joeUser.getId().toString())
                    .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                    .type(ProtobufProviders.APPLICATION_XPROTOBUF)
                    .post(ProjectInfo.class, newProject);
        }
        // Should fail
        newProject = NewProject.newBuilder()
                .setName("test project")
                .setDescription("New test project").build();
        try {
            joeProjectsWebResource
                    .path(joeUser.getId().toString())
                    .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                    .type(ProtobufProviders.APPLICATION_XPROTOBUF)
                    .post(ProjectInfo.class, newProject);
            assertTrue(false);
        } catch (UniformInterfaceException e) {
            assertThat(e.getResponse().getStatus(), is(400));
            return;
        }
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

    private static IGit httpAuthGit(TestUser testUser) {
        IGit git = GitFactory.create(GitFactory.Type.CGIT);
        git.setUsername(testUser.email);
        git.setPassword(testUser.password);
        git.setHost("localhost");
        return git;
    }

    private static IGit openIDGit(TestUser testUser) {
        IGit git = GitFactory.create(GitFactory.Type.CGIT);
        git.setUsername(testUser.email);
        git.setPassword(AuthCookie.login(testUser.email));
        git.setHost("localhost");
        return git;
    }

    private static String cloneRepoHttpAuth(TestUser testUser, ProjectInfo projectInfo) throws IOException {
        IGit git = httpAuthGit(testUser);
        File cloneDir = Files.createTempDir();
        git.cloneRepo(projectInfo.getRepositoryUrl(), cloneDir.getAbsolutePath());
        return cloneDir.getAbsolutePath();
    }

    private static String cloneRepoOpenID(TestUser testUser, ProjectInfo projectInfo) throws IOException {
        IGit git = openIDGit(testUser);
        File cloneDir = Files.createTempDir();
        git.cloneRepo(projectInfo.getRepositoryUrl(), cloneDir.getAbsolutePath());
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

    @Test(expected=GitException.class)
    public void cloneHTTPBasicAuthAccessDenied() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        cloneRepoHttpAuth(bob, projectInfo);
    }

    @Test(expected=GitException.class)
    public void cloneOpenIDAccessDenied() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        cloneRepoOpenID(bob, projectInfo);
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
        IGit git = httpAuthGit(joe);
        git.commitAll(cloneDir, "a commit");
        git.push(cloneDir);
    }

    @Test
    public void pushOpenID() throws Exception {
        ProjectInfo projectInfo = createTemplateProject(joe, "proj1");
        String cloneDir = cloneRepoHttpAuth(joe, projectInfo);
        alterFile(cloneDir, "content/file1.txt", "some content");
        IGit git = openIDGit(joe);
        git.commitAll(cloneDir, "a commit");
        git.push(cloneDir);
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

        response = joeProjectsWebResource.path(String.format("/%d", joeUser.getId())).get(ClientResponse.class);
        assertEquals(200, response.getStatus());

        response = bobProjectsWebResource.path(String.format("/%d", bobUser.getId())).get(ClientResponse.class);
        assertEquals(200, response.getStatus());

        response = bobProjectsWebResource.path(String.format("/%d", joeUser.getId())).get(ClientResponse.class);
        assertEquals(403, response.getStatus());
    }

    @Test
    public void testProjectStatus() throws Exception {
        // Setup is:
        // p1: joe, bob
        // p2: joe
        NewProject newProject = NewProject.newBuilder()
                .setName("p1")
                .setDescription("New test project").build();

        ProjectInfo projectInfo = joeProjectsWebResource
                .path(joeUser.getId().toString())
                .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                .type(ProtobufProviders.APPLICATION_XPROTOBUF)
                .post(ProjectInfo.class, newProject);

        // Add bob as member
        joeProjectsWebResource.path(String.format("/%d/%d/members", joeUser.getId(), projectInfo.getId()))
                .post(bobEmail);

        newProject = NewProject.newBuilder()
                .setName("p2")
                .setDescription("New test project").build();

        projectInfo = joeProjectsWebResource
                .path(joeUser.getId().toString())
                .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                .type(ProtobufProviders.APPLICATION_XPROTOBUF)
                .post(ProjectInfo.class, newProject);

        assertThat(projectInfo.getStatus(), is(ProjectStatus.PROJECT_STATUS_OK));

        ProjectInfoList list = joeProjectsWebResource
                .path(joeUser.getId().toString())
                .accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE)
                .get(ProjectInfoList.class);

        assertThat(list.getProjects(0).getStatus(), is(ProjectStatus.PROJECT_STATUS_UNQUALIFIED));
        assertThat(list.getProjects(1).getStatus(), is(ProjectStatus.PROJECT_STATUS_OK));
    }

}
