package com.dynamo.cr.server.auth;

import org.junit.Test;

import static org.junit.Assert.assertEquals;

public class DiscourseSSOAuthenticatorTest {

    @Test
    public void testDiscourseSSOAuthenticator() {
        /*
            This is an example from https://meta.discourse.org/t/official-single-sign-on-for-discourse/13045
            that should work.

            Secret: d836444a9e4084d5b224a60c208dce14
            Payload: bm9uY2U9Y2I2ODI1MWVlZmI1MjExZTU4YzAwZmYxMzk1ZjBjMGI=\n
            Signature: 2828aa29899722b35a2f191d34ef9b3ce695e0e6eeec47deb46d588d70c7cb56
            Nonce: cb68251eefb5211e58c00ff1395f0c0b
         */

        String secret = "d836444a9e4084d5b224a60c208dce14";
        String payload = "bm9uY2U9Y2I2ODI1MWVlZmI1MjExZTU4YzAwZmYxMzk1ZjBjMGI=\n";
        String signature = "2828aa29899722b35a2f191d34ef9b3ce695e0e6eeec47deb46d588d70c7cb56";

        DiscourseSSOAuthenticator authenticator = new DiscourseSSOAuthenticator(secret);

        // Validate signature
        assertEquals(signature, authenticator.generateSignature(payload));

        // Create response payload
        /*
            name: sam
            external_id: hello123
            email: test@test.com
            username: samsam
            require_activation: true
         */
        String respondPayload = authenticator.createResponsePayload(payload, "sam", "samsam", "test@test.com", "hello123", true);
        assertEquals("bm9uY2U9Y2I2ODI1MWVlZmI1MjExZTU4YzAwZmYxMzk1ZjBjMGImbmFtZT1zYW0mdXNlcm5hbWU9c2Ftc2FtJmVtYWlsPXRlc3QlNDB0ZXN0LmNvbSZleHRlcm5hbF9pZD1oZWxsbzEyMyZyZXF1aXJlX2FjdGl2YXRpb249dHJ1ZQ==", respondPayload);
    }
}