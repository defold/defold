package com.dynamo.cr.server.resources.test;

import static org.junit.Assert.assertEquals;

import java.io.IOException;
import java.net.URI;

import javax.persistence.EntityManager;
import javax.ws.rs.core.UriBuilder;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.common.providers.JsonProviders;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.server.dgit.CommandUtil;
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

    @Test
    public void testNewProspect() throws Exception {
        final String email = "test@foo.com";

        ClientResponse response;
        response = prospectsWebResource.path(String.format("/%s", email)).put(ClientResponse.class);
        assertEquals(ClientResponse.Status.OK.getStatusCode(), response.getStatus());
    }

    @Test
    public void testNewProspectDuplicate() throws Exception {
        final String email = "test@foo.com";

        ClientResponse response;
        response = prospectsWebResource.path(String.format("/%s", email)).put(ClientResponse.class);
        assertEquals(ClientResponse.Status.OK.getStatusCode(), response.getStatus());

        response = prospectsWebResource.path(String.format("/%s", email)).put(ClientResponse.class);
        assertEquals(ClientResponse.Status.CONFLICT.getStatusCode(), response.getStatus());
    }

}
