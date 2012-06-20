package com.dynamo.cr.server.resources.test;

import static org.junit.Assert.assertEquals;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URLEncoder;
import java.util.List;
import java.util.Map;

import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.UriBuilder;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.common.providers.JsonProviders;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.server.util.ChargifyUtil;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.WebResource.Builder;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.representation.Form;

public class ChargifyResourceTest extends AbstractResourceTest {

    int port = 6500;
    DefaultClientConfig clientConfig;
    WebResource chargifyResource;

    @Before
    public void setUp() throws Exception {
        setupUpTest();

        clientConfig = new DefaultClientConfig();
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyWriter.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        Client client = Client.create(clientConfig);

        URI uri = UriBuilder.fromUri(String.format("http://localhost/chargify")).port(port).build();
        chargifyResource = client.resource(uri);
    }

    private String generateSignature(Form f) throws UnsupportedEncodingException {
        // Format the form into the corresponding HTTP request body
        // Could not find a nice way to do this in jersey
        StringBuffer buffer = new StringBuffer();
        boolean empty = true;
        for (Map.Entry<String, List<String>> entry : f.entrySet()) {
            for (String value : entry.getValue()) {
                if (!empty) {
                    buffer.append("&");
                }
                String key = URLEncoder.encode(entry.getKey(), "UTF-8");
                value = URLEncoder.encode(value, "UTF-8");
                buffer.append(key).append("=").append(value);
                empty = false;
            }
        }
        String body = buffer.toString();
        return new String(ChargifyUtil.generateSignature(server.getConfiguration().getBillingSharedKey().getBytes(),
                body.getBytes()));
    }

    private ClientResponse post(String event, String site, String subscription, boolean sign)
            throws UnsupportedEncodingException {
        Form f = new Form();
        f.add("event", event);
        f.add("payload[site]", site);
        f.add("payload[subscription]", subscription);

        Builder builder = chargifyResource.type(MediaType.APPLICATION_FORM_URLENCODED_TYPE)
                .accept(MediaType.APPLICATION_JSON_TYPE).entity(f);
        if (sign) {
            String signature = generateSignature(f);
            builder.header(ChargifyUtil.SIGNATURE_HEADER_NAME, signature);
        }
        return builder.post(ClientResponse.class);
    }

    @Test
    public void testAccess() throws Exception {
        ClientResponse response = post("signup_success", "the_site", "the_subs", true);
        assertEquals(ClientResponse.Status.NO_CONTENT.getStatusCode(), response.getClientResponseStatus()
                .getStatusCode());
    }

    @Test
    public void testNoAccess() throws Exception {
        ClientResponse response = post("signup_success", "the_site", "the_subs", false);
        assertEquals(ClientResponse.Status.FORBIDDEN.getStatusCode(), response.getClientResponseStatus()
                .getStatusCode());
    }
}

