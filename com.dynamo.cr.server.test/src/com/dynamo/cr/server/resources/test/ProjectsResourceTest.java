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
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfoList;
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

    int port = 6500;
    String joeEmail = "joe@foo.com";
    String joePasswd = "secret2";
    String bobEmail = "bob@foo.com";
    String bobPasswd = "secret3";
    String adminEmail = "admin@foo.com";
    String adminPasswd = "secret";
    private User joeUser;
    private User adminUser;

    private WebResource adminProjectsWebResource;
    private WebResource joeProjectsWebResource;

    private User bobUser;

    void execCommand(String command, String arg) throws IOException {
        CommandUtil.Result r = CommandUtil.execCommand(new String[] {"sh", command, arg});
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

        Client client = Client.create(cc);
        client.addFilter(new HTTPBasicAuthFilter(adminEmail, adminPasswd));

        URI uri = UriBuilder.fromUri(String.format("http://localhost/projects")).port(port).build();
        adminProjectsWebResource = client.resource(uri);

        client = Client.create(cc);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));

        uri = UriBuilder.fromUri(String.format("http://localhost/projects")).port(port).build();
        joeProjectsWebResource = client.resource(uri);
    }

    @After
    public void tearDown() throws Exception {
        server.stop();
    }

    @Test
    public void newProject() throws Exception {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        TypedQuery<Project> query = em.createQuery("select p from Project p", Project.class);
        int nprojects = query.getResultList().size();

        ObjectMapper m = new ObjectMapper();
        ObjectNode project = m.createObjectNode();
        project.put("name", "test project");
        project.put("description", "New test project");

        ProjectInfo projectInfo = joeProjectsWebResource
            .path(joeUser.getId().toString())
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .post(ProjectInfo.class, project.toString());

        // Add bob as member
        joeProjectsWebResource.path(String.format("/%d/%d/members", joeUser.getId(), projectInfo.getId()))
            .post(bobEmail);

        assertEquals("test project", projectInfo.getName());
        assertEquals(joeUser.getId().longValue(), projectInfo.getOwner().getId());
        assertEquals(joeUser.getEmail(), projectInfo.getOwner().getEmail());

        ProjectInfoList list = joeProjectsWebResource
            .path(joeUser.getId().toString())
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .get(ProjectInfoList.class);

        assertEquals(joeUser.getId().longValue(), list.getProjects(0).getOwner().getId());
        assertEquals(nprojects + 1, query.getResultList().size());
        assertEquals(2, list.getProjects(0).getMembersCount());

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
