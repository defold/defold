package com.dynamo.cr.server.resources.test;

import com.dynamo.cr.server.model.User;
import com.sun.jersey.api.client.WebResource;
import org.junit.Test;

import static org.junit.Assert.assertTrue;

public class AccessTokenResourceTest extends AbstractResourceTest {

    @Test
    public void userShouldBeAbleToGenerateNewAccessToken() {
        User bobUser = createUser("bob@foo.com", "bob", "Mr", "Bob", User.Role.USER);

        WebResource resource = createBaseResource(bobUser, "bob");
        String access_token = resource.path("access_token").post(String.class);

        assertTrue(access_token != null && !access_token.isEmpty());
    }

}
