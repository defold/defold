package com.dynamo.cr.server.auth.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import java.io.ByteArrayInputStream;
import java.nio.charset.StandardCharsets;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.server.auth.OAuthAuthenticator;
import com.dynamo.cr.server.auth.OAuthAuthenticator.Authentication;
import com.dynamo.cr.server.model.User;
import com.google.api.client.http.HttpRequestFactory;
import com.google.api.client.testing.http.MockHttpTransport;
import com.google.api.client.testing.http.MockLowLevelHttpResponse;

public class OAuthAuthenticatorTest {

    private OAuthAuthenticator authenticator;
    private final int maxActiveLogins = 1;

    @Before
    public void setUp() throws Exception {
        authenticator = new OAuthAuthenticator(maxActiveLogins);
    }

    @After
    public void tearDown() throws Exception {
    }


    @Test
    public void testGetAuthToken() throws Exception {
        User user = new User();
        user.setEmail("bob@coder.com");

        String token = authenticator.getAuthToken(user);
        assertEquals(40, token.length());
    }

    @Test
    public void testExchangeToken() throws Exception {
        String loginToken = authenticator.newLoginToken();
        String accessToken = "SplxlOBeZQQYbYS6WxSbIA";
        String bobEmail = "bob@coder.com";
        String identityString = "{\"id\":\"identity id\", \"verified_email\": true"
                + ", \"email\":\"" + bobEmail + "\", \"given_name\":\"Bob\", \"family_name\":\"Coder\"}";

        MockLowLevelHttpResponse mockResponse = new MockLowLevelHttpResponse();
        mockResponse.setContent(new ByteArrayInputStream(identityString.getBytes(StandardCharsets.UTF_8)));

        HttpRequestFactory mockFactory = new MockHttpTransport.Builder().setLowLevelHttpResponse(mockResponse).build().createRequestFactory();

        authenticator.authenticate(mockFactory, loginToken, accessToken, "test_jwt");

        Authentication authentication = authenticator.exchange(loginToken);
        assertEquals(accessToken, authentication.accessToken);
        assertEquals(bobEmail, authentication.identity.email);
    }

    @Test
    public void testExchangeInvalidToken() throws Exception {
        String loginToken = authenticator.newLoginToken();

        Authentication authentication = authenticator.exchange(loginToken);
        assertNull(authentication);
    }

}
