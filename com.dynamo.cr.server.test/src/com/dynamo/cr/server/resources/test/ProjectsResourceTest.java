package com.dynamo.cr.server.resources.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;
import java.net.URI;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.persistence.TypedQuery;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.UriBuilder;

import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ObjectNode;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.common.providers.JsonProviders;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.protocol.proto.Protocol.NewProject;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfoList;
import com.dynamo.cr.protocol.proto.Protocol.UserInfoList;
import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.test.Util;
import com.dynamo.server.git.CommandUtil;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;


public class ProjectsResourceTest {
    private Server server;

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
        // "drop-and-create-tables" can't handle model changes correctly. We need to drop all tables first.
        // Eclipse-link only drops tables currently specified. When the model change the table set also change.
        File tmp_testdb = new File("tmp/testdb");
        if (tmp_testdb.exists()) {
            getClass().getClassLoader().loadClass("org.apache.derby.jdbc.EmbeddedDriver");
            Util.dropAllTables();
        }

        execCommand("scripts/setup_template_project.sh", "proj1");

        server = new Server("test_data/crepo_test.config");

        EntityManagerFactory emf = server.getEntityManagerFactory();
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
    }

    @After
    public void tearDown() throws Exception {
        adminClient.destroy();
        joeClient.destroy();
        bobClient.destroy();
        server.stop();
    }

    @Test
    public void testProjectInfo() throws Exception {
        NewProject newProject = NewProject.newBuilder()
                .setName("test project")
                .setDescription("New test project").build();

        ProjectInfo projectInfo = joeProjectsWebResource
            .path(joeUser.getId().toString())
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .type(ProtobufProviders.APPLICATION_XPROTOBUF)
            .post(ProjectInfo.class, newProject);

        ClientResponse response;
        response = joeProjectsWebResource.path(String.format("/%d/%d/project_info", bobUser.getId(), projectInfo.getId())).get(ClientResponse.class);
        assertEquals(200, response.getStatus());
        response.getEntity(ProjectInfo.class);
    }

    @Test
    public void testProjectInfoJsonp() throws Exception {
        NewProject newProject = NewProject.newBuilder()
                .setName("test project")
                .setDescription("New test project").build();

        ProjectInfo projectInfo = joeProjectsWebResource
            .path(joeUser.getId().toString())
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .type(ProtobufProviders.APPLICATION_XPROTOBUF)
            .post(ProjectInfo.class, newProject);



        ClientResponse response;
        response = joeProjectsWebResource.path(String.format("/%d/%d/project_info", bobUser.getId(), projectInfo.getId())).queryParam("callback", "foo").accept(MediaType.APPLICATION_JSON_TYPE).get(ClientResponse.class);
        assertEquals(200, response.getStatus());
        String projectInfoJsonp = response.getEntity(String.class);
        assertEquals("foo(", projectInfoJsonp.substring(0, 4));
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

        // Check that bob now is connected to joe
        assertEquals(1, bobsConnections.getUsersCount());
        assertEquals(joeEmail, bobsConnections.getUsers(0).getEmail());

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

        assertEquals(400, response.getStatus());
        // Make sure that no project is left in the database
        assertEquals(nprojects, query.getResultList().size());
        em.close();
    }
}
