package com.dynamo.cr.server.resources.test;

import static org.junit.Assert.assertEquals;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.net.URI;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.UriBuilder;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ObjectNode;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.common.providers.JsonProviders;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.test.Util;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;

public class UsersResourceTest {

    private Server server;

    int port = 6500;
    String joeEmail = "joe@foo.com";
    String joePasswd = "secret2";
    String adminEmail = "admin@foo.com";
    String adminPasswd = "secret";
    private User user;
    private User adminUser;
    private WebResource adminUsersWebResource;

    private WebResource joeUsersWebResource;

    @Before
    public void setUp() throws Exception {
        // "drop-and-create-tables" can't handle model changes correctly. We need to drop all tables first.
        // Eclipse-link only drops tables currently specified. When the model change the table set also change.
        File tmp_testdb = new File("tmp/testdb");
        if (tmp_testdb.exists()) {
            getClass().getClassLoader().loadClass("org.apache.derby.jdbc.EmbeddedDriver");
            Util.dropAllTables();
        }

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

        user = new User();
        user.setEmail(joeEmail);
        user.setFirstName("undefined");
        user.setLastName("undefined");
        user.setPassword(joePasswd);
        user.setRole(Role.USER);
        em.persist(user);

        em.getTransaction().commit();

        ClientConfig cc = new DefaultClientConfig();
        cc.getClasses().add(JsonProviders.ProtobufMessageBodyReader.class);
        cc.getClasses().add(JsonProviders.ProtobufMessageBodyWriter.class);

        Client client = Client.create(cc);
        client.addFilter(new HTTPBasicAuthFilter(adminEmail, adminPasswd));

        URI uri = UriBuilder.fromUri(String.format("http://localhost/users")).port(port).build();
        adminUsersWebResource = client.resource(uri);

        client = Client.create(cc);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));

        uri = UriBuilder.fromUri(String.format("http://localhost/users")).port(port).build();
        joeUsersWebResource = client.resource(uri);
    }

    @After
    public void tearDown() throws Exception {
        server.stop();
    }

    @Test
    public void testGetUserInfo() throws Exception {
        UserInfo userInfo = adminUsersWebResource
            .path("joe@foo.com")
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .get(UserInfo.class);

        assertEquals("joe@foo.com", userInfo.getEmail());
    }

    @Test
    public void testGetUserInfoNotRegistred() throws Exception {
        ClientResponse response = adminUsersWebResource
            .path("notregistred@foo.com")
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .get(ClientResponse.class);

        assertEquals(404, response.getStatus());
    }

    @Test
    public void testRegister() throws Exception {
        ObjectMapper m = new ObjectMapper();
        ObjectNode user = m.createObjectNode();
        user.put("email", "test@foo.com");
        user.put("first_name", "mr");
        user.put("last_name", "test");
        user.put("password", "verysecret");

        UserInfo userInfo = adminUsersWebResource
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .post(UserInfo.class, user.toString());

        assertEquals("test@foo.com", userInfo.getEmail());
        assertEquals("mr", userInfo.getFirstName());
        assertEquals("test", userInfo.getLastName());
    }

    @Test
    public void testRegisterTextAccept() throws Exception {
        ObjectMapper m = new ObjectMapper();
        ObjectNode user = m.createObjectNode();
        user.put("email", "test@foo.com");
        user.put("first_name", "mr");
        user.put("last_name", "test");
        user.put("password", "verysecret");

        String userInfoText = adminUsersWebResource
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .post(String.class, user.toString());

        ObjectMapper om = new ObjectMapper();
        JsonNode node = om.readValue(new ByteArrayInputStream(userInfoText.getBytes()), JsonNode.class);

        assertEquals("test@foo.com", node.get("email").getTextValue());
        assertEquals("mr", node.get("first_name").getTextValue());
        assertEquals("test", node.get("last_name").getTextValue());
    }

    @Test
    public void testRegisterAsJoe() throws Exception {
        ObjectMapper m = new ObjectMapper();
        ObjectNode user = m.createObjectNode();
        user.put("email", "test@foo.com");
        user.put("first_name", "mr");
        user.put("last_name", "test");
        user.put("password", "verysecret");

        ClientResponse response = joeUsersWebResource
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .post(ClientResponse.class, user.toString());
        assertEquals(403, response.getStatus());
    }

}
