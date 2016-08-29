package com.dynamo.cr.server.auth;

import com.dynamo.cr.server.auth.OAuthAuthenticator;
import com.dynamo.cr.server.auth.OAuthAuthenticator.Authentication;
import com.google.api.client.http.HttpRequestFactory;
import com.google.api.client.testing.http.MockHttpTransport;
import com.google.api.client.testing.http.MockLowLevelHttpResponse;
import org.junit.Before;
import org.junit.Test;

import java.io.ByteArrayInputStream;
import java.nio.charset.StandardCharsets;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

public class OAuthAuthenticatorTest {
    private static final int MAX_ACTIVE_LOGINS = 1;
    private OAuthAuthenticator authenticator;

    @Before
    public void setUp() throws Exception {
        authenticator = new OAuthAuthenticator(MAX_ACTIVE_LOGINS);
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

        authenticator.authenticate(mockFactory, loginToken, accessToken);

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
