package com.dynamo.cr.server.resources;

import com.sun.jersey.api.client.WebResource;
import org.junit.Test;

import static org.junit.Assert.assertEquals;

public class StatusResourceTest extends AbstractResourceTest {

    @Test
    public void serverStatusShouldBeOk() throws Exception {
        WebResource statusResource = createAnonymousResource("http://localhost/status");
        String statusResponse = statusResource.get(String.class);
        assertEquals("OK", statusResponse);
    }
}
