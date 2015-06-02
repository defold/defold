package com.dynamo.cr.server.resources.test;

import static org.junit.Assert.assertEquals;

import java.io.IOException;
import java.net.URI;
import java.util.List;

import javax.persistence.EntityManager;
import javax.persistence.Query;
import javax.ws.rs.core.UriBuilder;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.server.model.Invitation;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.providers.JsonProviders;
import com.dynamo.cr.server.providers.ProtobufProviders;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;


public class ProspectsResourceTest extends AbstractResourceTest {

    static int port = 6500;

    private WebResource prospectsWebResource;

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
        TestUtil.Result r = TestUtil.execCommand(new String[] {"/bin/bash", command, arg});
        if (r.exitValue != 0) {
            System.err.println(r.stdOut);
            System.err.println(r.stdErr);
        }
        assertEquals(0, r.exitValue);
    }

    @Before
    public void setUp() throws Exception {
        setupUpTest();

        EntityManager em = emf.createEntityManager();
        em.getTransaction().begin();

        ClientConfig cc = new DefaultClientConfig();
        cc.getClasses().add(JsonProviders.ProtobufMessageBodyReader.class);
        cc.getClasses().add(JsonProviders.ProtobufMessageBodyWriter.class);
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        Client client = Client.create(cc);

        URI uri = UriBuilder.fromUri(String.format("http://localhost/prospects")).port(port).build();
        prospectsWebResource = client.resource(uri);
    }

    void sleep() throws InterruptedException {
        // Arbitrary sleep for mail processor
        Thread.sleep(200);
    }

    @SuppressWarnings("unchecked")
    List<Invitation> invitations(EntityManager em) {
        return em.createQuery("select i from Invitation i").getResultList();
    }

    @Test
    public void testNewProspect() throws Exception {
        EntityManager em = emf.createEntityManager();
        assertEquals(0, mailer.emails.size());
        final String email = "test@foo.com";

        ClientResponse response;
        response = prospectsWebResource.path(String.format("/%s", email)).put(ClientResponse.class);
        assertEquals(ClientResponse.Status.OK.getStatusCode(), response.getStatus());
        sleep();
        assertEquals(1, mailer.emails.size());
        assertEquals(1, invitations(em).size());

        em.close();
    }

    @Test
    public void testNewProspectDuplicate() throws Exception {
        EntityManager em = emf.createEntityManager();

        assertEquals(0, mailer.emails.size());
        final String email = "test@foo.com";

        ClientResponse response;
        response = prospectsWebResource.path(String.format("/%s", email)).put(ClientResponse.class);
        assertEquals(ClientResponse.Status.OK.getStatusCode(), response.getStatus());
        sleep();
        assertEquals(1, mailer.emails.size());
        assertEquals(1, invitations(em).size());

        response = prospectsWebResource.path(String.format("/%s", email)).put(ClientResponse.class);
        assertEquals(ClientResponse.Status.CONFLICT.getStatusCode(), response.getStatus());
        sleep();
        assertEquals(1, mailer.emails.size());
        assertEquals(1, invitations(em).size());

        em.close();
    }

    @Test
    public void testNewProspectCap() throws Exception {
        EntityManager em = emf.createEntityManager();

        assertEquals(0, mailer.emails.size());
        assertEquals(0, invitations(em).size());

        Query query = em.createQuery("select count(u.id) from User u");
        int userCount = ((Number) query.getSingleResult()).intValue();
        int maxUsers = server.getConfiguration().getOpenRegistrationMaxUsers();

        // Create users up to maxUsers
        em.getTransaction().begin();
        while (userCount < maxUsers) {
            User user = new User();
            user.setPassword("pass");
            user.setEmail(String.format("foo%d@bar.com", userCount));
            user.setFirstName(String.format("first", userCount));
            user.setLastName(String.format("last", userCount));
            em.persist(user);
            userCount++;
        }
        em.getTransaction().commit();

        ClientResponse response;
        final String email = "test@foo.com";
        response = prospectsWebResource.path(String.format("/%s", email)).put(ClientResponse.class);
        assertEquals(ClientResponse.Status.OK.getStatusCode(), response.getStatus());
        sleep();
        assertEquals(0, mailer.emails.size());
        assertEquals(0, invitations(em).size());

        em.close();
    }

}
