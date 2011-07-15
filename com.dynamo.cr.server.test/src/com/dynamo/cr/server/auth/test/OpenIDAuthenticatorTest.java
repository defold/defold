package com.dynamo.cr.server.auth.test;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.net.URI;
import java.net.URL;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.server.auth.OpenIDAuthenticator;
import com.dynamo.cr.server.openid.Association;
import com.dynamo.cr.server.openid.EndPoint;
import com.dynamo.cr.server.openid.IAssociator;
import com.dynamo.cr.server.openid.IEndPointResolver;
import com.dynamo.cr.server.openid.OpenID;
import com.dynamo.cr.server.openid.OpenIDException;


public class OpenIDAuthenticatorTest {

    final static String testAssociationResponse = "ns:http://specs.openid.net/auth/2.0\n"
        + "session_type:no-encryption\n" + "assoc_type:HMAC-SHA1\n"
        + "assoc_handle:AOQobUfFOaDc6zFQ4PaGqJt5oNLes__P0CFci9gOxzHLqHX29hDFWjpD\n"
        + "expires_in:46800\n" + "mac_key:JdeP/doei+dloEhISpOoPHD1mIk=\n";

    final static String testAuthenticationResponse = "http://localhost:9998/login/openid/google/response/7d88f5d58fd8cae86aa18c83789e7e62?openid.ns=http%3A%2F%2Fspecs.openid.net%2Fauth%2F2.0&openid.mode=id_res&openid.op_endpoint=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fud&openid.response_nonce=2011-07-07T17%3A36%3A14Zr_hKJMfgZaPPDQ&openid.return_to=http%3A%2F%2Flocalhost%3A9998%2Flogin%2Fopenid%2Fgoogle%2Fresponse%2F7d88f5d58fd8cae86aa18c83789e7e62&openid.assoc_handle=AOQobUfFOaDc6zFQ4PaGqJt5oNLes__P0CFci9gOxzHLqHX29hDFWjpD&openid.signed=op_endpoint%2Cclaimed_id%2Cidentity%2Creturn_to%2Cresponse_nonce%2Cassoc_handle%2Cns.ext1%2Cext1.mode%2Cext1.type.firstname%2Cext1.value.firstname%2Cext1.type.email%2Cext1.value.email%2Cext1.type.language%2Cext1.value.language%2Cext1.type.lastname%2Cext1.value.lastname&openid.sig=8CtXE5QAHUF06oUpN69tE0LkOCA%3D&openid.identity=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fid%3Fid%3DAItOawlQCggILp_hoDD41sA1Fw2o9kEEnF9a9mk&openid.claimed_id=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fid%3Fid%3DAItOawlQCggILp_hoDD41sA1Fw2o9kEEnF9a9mk&openid.ns.ext1=http%3A%2F%2Fopenid.net%2Fsrv%2Fax%2F1.0&openid.ext1.mode=fetch_response&openid.ext1.type.firstname=http%3A%2F%2Faxschema.org%2FnamePerson%2Ffirst&openid.ext1.value.firstname=Christian&openid.ext1.type.email=http%3A%2F%2Faxschema.org%2Fcontact%2Femail&openid.ext1.value.email=christian.murray%40defold.se&openid.ext1.type.language=http%3A%2F%2Faxschema.org%2Fpref%2Flanguage&openid.ext1.value.language=sv&openid.ext1.type.lastname=http%3A%2F%2Faxschema.org%2FnamePerson%2Flast&openid.ext1.value.lastname=Murray";
    final static String testAuthenticationResponseTamperEmail = "http://localhost:9998/login/openid/google/response/7d88f5d58fd8cae86aa18c83789e7e62?openid.ns=http%3A%2F%2Fspecs.openid.net%2Fauth%2F2.0&openid.mode=id_res&openid.op_endpoint=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fud&openid.response_nonce=2011-07-07T17%3A36%3A14Zr_hKJMfgZaPPDQ&openid.return_to=http%3A%2F%2Flocalhost%3A9998%2Flogin%2Fopenid%2Fgoogle%2Fresponse%2F7d88f5d58fd8cae86aa18c83789e7e62&openid.assoc_handle=AOQobUfFOaDc6zFQ4PaGqJt5oNLes__P0CFci9gOxzHLqHX29hDFWjpD&openid.signed=op_endpoint%2Cclaimed_id%2Cidentity%2Creturn_to%2Cresponse_nonce%2Cassoc_handle%2Cns.ext1%2Cext1.mode%2Cext1.type.firstname%2Cext1.value.firstname%2Cext1.type.email%2Cext1.value.email%2Cext1.type.language%2Cext1.value.language%2Cext1.type.lastname%2Cext1.value.lastname&openid.sig=8CtXE5QAHUF06oUpN69tE0LkOCA%3D&openid.identity=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fid%3Fid%3DAItOawlQCggILp_hoDD41sA1Fw2o9kEEnF9a9mk&openid.claimed_id=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fid%3Fid%3DAItOawlQCggILp_hoDD41sA1Fw2o9kEEnF9a9mk&openid.ns.ext1=http%3A%2F%2Fopenid.net%2Fsrv%2Fax%2F1.0&openid.ext1.mode=fetch_response&openid.ext1.type.firstname=http%3A%2F%2Faxschema.org%2FnamePerson%2Ffirst&openid.ext1.value.firstname=Christian&openid.ext1.type.email=http%3A%2F%2Faxschema.org%2Fcontact%2Femail&openid.ext1.value.email=christian.murray%40dekorv.se&openid.ext1.type.language=http%3A%2F%2Faxschema.org%2Fpref%2Flanguage&openid.ext1.value.language=sv&openid.ext1.type.lastname=http%3A%2F%2Faxschema.org%2FnamePerson%2Flast&openid.ext1.value.lastname=Murray";

    private OpenID openId;

    @Before
    public void setUp() throws Exception {
        openId = new OpenID();

        IEndPointResolver resolver = mock(IEndPointResolver.class);
        openId.setEndPointResolver(resolver);

        IAssociator associator = mock(IAssociator.class);
        when(associator.associate(any(EndPoint.class))).thenReturn(
                Association.create(testAssociationResponse));
        openId.setAssociator(associator);

        long expires_in = 3600;
        when(resolver.resolve("google")).thenReturn(
                new EndPoint(new URL("http://example.com"), expires_in));
    }

    @Test
    public void testAuthenticated() throws Exception {
        @SuppressWarnings("unused")
        String url = openId.getAuthenticationUrl("google", "http://localhost", "http://localhost/auth");

        OpenIDAuthenticator openIdAuth = new OpenIDAuthenticator(1024);
        String loginToken = openIdAuth.newLoginToken();
        openIdAuth.authenticate(openId, new URI(testAuthenticationResponse), loginToken);
    }

    @Test(expected = OpenIDException.class)
    public void testInvalidToken() throws Exception {
        @SuppressWarnings("unused")
        String url = openId.getAuthenticationUrl("google", "http://localhost", "http://localhost/auth");

        OpenIDAuthenticator openIdAuth = new OpenIDAuthenticator(1024);
        openIdAuth.authenticate(openId, new URI(testAuthenticationResponse), "foobar");
    }

    @Test(expected = OpenIDException.class)
    public void testSignatureVerificationError() throws Exception {
        @SuppressWarnings("unused")
        String url = openId.getAuthenticationUrl("google", "http://localhost", "http://localhost/auth");

        OpenIDAuthenticator openIdAuth = new OpenIDAuthenticator(1024);
        String loginToken = openIdAuth.newLoginToken();
        openIdAuth.authenticate(openId, new URI(testAuthenticationResponseTamperEmail), loginToken);
    }
}
