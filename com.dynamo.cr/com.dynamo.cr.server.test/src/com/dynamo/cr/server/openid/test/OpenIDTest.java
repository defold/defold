package com.dynamo.cr.server.openid.test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.net.URI;
import java.net.URL;

import org.apache.commons.codec.binary.Base64;
import org.junit.Test;

import com.dynamo.cr.server.openid.Association;
import com.dynamo.cr.server.openid.EndPoint;
import com.dynamo.cr.server.openid.IAssociator;
import com.dynamo.cr.server.openid.IEndPointResolver;
import com.dynamo.cr.server.openid.OpenID;
import com.dynamo.cr.server.openid.OpenIDException;

public class OpenIDTest {

    /*
     * Test data based on an authentication request to google
     *
     * Association response from google
     *
     * ns:http://specs.openid.net/auth/2.0
     * session_type:no-encryption
     * assoc_type:HMAC-SHA1
     * assoc_handle:AOQobUfFOaDc6zFQ4PaGqJt5oNLes__P0CFci9gOxzHLqHX29hDFWjpD
     * expires_in:46800
     * mac_key:JdeP/doei+dloEhISpOoPHD1mIk=
     *
     * Authentcation url response from google (openid.return_to)
     * http://localhost:9998/login/openid/google/response/7d88f5d58fd8cae86aa18c83789e7e62?openid.ns=http%3A%2F%2Fspecs.openid.net%2Fauth%2F2.0&openid.mode=id_res&openid.op_endpoint=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fud&openid.response_nonce=2011-07-07T17%3A36%3A14Zr_hKJMfgZaPPDQ&openid.return_to=http%3A%2F%2Flocalhost%3A9998%2Flogin%2Fopenid%2Fgoogle%2Fresponse%2F7d88f5d58fd8cae86aa18c83789e7e62&openid.assoc_handle=AOQobUfFOaDc6zFQ4PaGqJt5oNLes__P0CFci9gOxzHLqHX29hDFWjpD&openid.signed=op_endpoint%2Cclaimed_id%2Cidentity%2Creturn_to%2Cresponse_nonce%2Cassoc_handle%2Cns.ext1%2Cext1.mode%2Cext1.type.firstname%2Cext1.value.firstname%2Cext1.type.email%2Cext1.value.email%2Cext1.type.language%2Cext1.value.language%2Cext1.type.lastname%2Cext1.value.lastname&openid.sig=8CtXE5QAHUF06oUpN69tE0LkOCA%3D&openid.identity=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fid%3Fid%3DAItOawlQCggILp_hoDD41sA1Fw2o9kEEnF9a9mk&openid.claimed_id=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fid%3Fid%3DAItOawlQCggILp_hoDD41sA1Fw2o9kEEnF9a9mk&openid.ns.ext1=http%3A%2F%2Fopenid.net%2Fsrv%2Fax%2F1.0&openid.ext1.mode=fetch_response&openid.ext1.type.firstname=http%3A%2F%2Faxschema.org%2FnamePerson%2Ffirst&openid.ext1.value.firstname=Christian&openid.ext1.type.email=http%3A%2F%2Faxschema.org%2Fcontact%2Femail&openid.ext1.value.email=christian.murray%40defold.se&openid.ext1.type.language=http%3A%2F%2Faxschema.org%2Fpref%2Flanguage&openid.ext1.value.language=sv&openid.ext1.type.lastname=http%3A%2F%2Faxschema.org%2FnamePerson%2Flast&openid.ext1.value.lastname=Murray
     *
     */

    final static String testAssociationResponse = "ns:http://specs.openid.net/auth/2.0\n"
            + "session_type:no-encryption\n" + "assoc_type:HMAC-SHA1\n"
            + "assoc_handle:AOQobUfFOaDc6zFQ4PaGqJt5oNLes__P0CFci9gOxzHLqHX29hDFWjpD\n"
            + "expires_in:46800\n" + "mac_key:JdeP/doei+dloEhISpOoPHD1mIk=\n";

    final static String testAssociationResponseExpiresIn1 = "ns:http://specs.openid.net/auth/2.0\n"
        + "session_type:no-encryption\n" + "assoc_type:HMAC-SHA1\n"
        + "assoc_handle:AOQobUfFOaDc6zFQ4PaGqJt5oNLes__P0CFci9gOxzHLqHX29hDFWjpD\n"
        + "expires_in:1\n" + "mac_key:JdeP/doei+dloEhISpOoPHD1mIk=\n";

    final static String testAuthenticationResponse = "http://localhost:9998/login/openid/google/response/7d88f5d58fd8cae86aa18c83789e7e62?openid.ns=http%3A%2F%2Fspecs.openid.net%2Fauth%2F2.0&openid.mode=id_res&openid.op_endpoint=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fud&openid.response_nonce=2011-07-07T17%3A36%3A14Zr_hKJMfgZaPPDQ&openid.return_to=http%3A%2F%2Flocalhost%3A9998%2Flogin%2Fopenid%2Fgoogle%2Fresponse%2F7d88f5d58fd8cae86aa18c83789e7e62&openid.assoc_handle=AOQobUfFOaDc6zFQ4PaGqJt5oNLes__P0CFci9gOxzHLqHX29hDFWjpD&openid.signed=op_endpoint%2Cclaimed_id%2Cidentity%2Creturn_to%2Cresponse_nonce%2Cassoc_handle%2Cns.ext1%2Cext1.mode%2Cext1.type.firstname%2Cext1.value.firstname%2Cext1.type.email%2Cext1.value.email%2Cext1.type.language%2Cext1.value.language%2Cext1.type.lastname%2Cext1.value.lastname&openid.sig=8CtXE5QAHUF06oUpN69tE0LkOCA%3D&openid.identity=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fid%3Fid%3DAItOawlQCggILp_hoDD41sA1Fw2o9kEEnF9a9mk&openid.claimed_id=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fid%3Fid%3DAItOawlQCggILp_hoDD41sA1Fw2o9kEEnF9a9mk&openid.ns.ext1=http%3A%2F%2Fopenid.net%2Fsrv%2Fax%2F1.0&openid.ext1.mode=fetch_response&openid.ext1.type.firstname=http%3A%2F%2Faxschema.org%2FnamePerson%2Ffirst&openid.ext1.value.firstname=Christian&openid.ext1.type.email=http%3A%2F%2Faxschema.org%2Fcontact%2Femail&openid.ext1.value.email=christian.murray%40defold.se&openid.ext1.type.language=http%3A%2F%2Faxschema.org%2Fpref%2Flanguage&openid.ext1.value.language=sv&openid.ext1.type.lastname=http%3A%2F%2Faxschema.org%2FnamePerson%2Flast&openid.ext1.value.lastname=Murray";
    final static String testAuthenticationResponseTamperEmail = "http://localhost:9998/login/openid/google/response/7d88f5d58fd8cae86aa18c83789e7e62?openid.ns=http%3A%2F%2Fspecs.openid.net%2Fauth%2F2.0&openid.mode=id_res&openid.op_endpoint=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fud&openid.response_nonce=2011-07-07T17%3A36%3A14Zr_hKJMfgZaPPDQ&openid.return_to=http%3A%2F%2Flocalhost%3A9998%2Flogin%2Fopenid%2Fgoogle%2Fresponse%2F7d88f5d58fd8cae86aa18c83789e7e62&openid.assoc_handle=AOQobUfFOaDc6zFQ4PaGqJt5oNLes__P0CFci9gOxzHLqHX29hDFWjpD&openid.signed=op_endpoint%2Cclaimed_id%2Cidentity%2Creturn_to%2Cresponse_nonce%2Cassoc_handle%2Cns.ext1%2Cext1.mode%2Cext1.type.firstname%2Cext1.value.firstname%2Cext1.type.email%2Cext1.value.email%2Cext1.type.language%2Cext1.value.language%2Cext1.type.lastname%2Cext1.value.lastname&openid.sig=8CtXE5QAHUF06oUpN69tE0LkOCA%3D&openid.identity=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fid%3Fid%3DAItOawlQCggILp_hoDD41sA1Fw2o9kEEnF9a9mk&openid.claimed_id=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fid%3Fid%3DAItOawlQCggILp_hoDD41sA1Fw2o9kEEnF9a9mk&openid.ns.ext1=http%3A%2F%2Fopenid.net%2Fsrv%2Fax%2F1.0&openid.ext1.mode=fetch_response&openid.ext1.type.firstname=http%3A%2F%2Faxschema.org%2FnamePerson%2Ffirst&openid.ext1.value.firstname=Christian&openid.ext1.type.email=http%3A%2F%2Faxschema.org%2Fcontact%2Femail&openid.ext1.value.email=christian.murray%40dekorv.se&openid.ext1.type.language=http%3A%2F%2Faxschema.org%2Fpref%2Flanguage&openid.ext1.value.language=sv&openid.ext1.type.lastname=http%3A%2F%2Faxschema.org%2FnamePerson%2Flast&openid.ext1.value.lastname=Murray";
    final static String testAuthenticationResponseTamperIdentity = "http://localhost:9998/login/openid/google/response/7d88f5d58fd8cae86aa18c83789e7e62?openid.ns=http%3A%2F%2Fspecs.openid.net%2Fauth%2F2.0&openid.mode=id_res&openid.op_endpoint=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fud&openid.response_nonce=2011-07-07T17%3A36%3A14Zr_hKJMfgZaPPDQ&openid.return_to=http%3A%2F%2Flocalhost%3A9998%2Flogin%2Fopenid%2Fgoogle%2Fresponse%2F7d88f5d58fd8cae86aa18c83789e7e62&openid.assoc_handle=AOQobUfFOaDc6zFQ4PaGqJt5oNLes__P0CFci9gOxzHLqHX29hDFWjpD&openid.signed=op_endpoint%2Cclaimed_id%2Cidentity%2Creturn_to%2Cresponse_nonce%2Cassoc_handle%2Cns.ext1%2Cext1.mode%2Cext1.type.firstname%2Cext1.value.firstname%2Cext1.type.email%2Cext1.value.email%2Cext1.type.language%2Cext1.value.language%2Cext1.type.lastname%2Cext1.value.lastname&openid.sig=8CtXE5QAHUF06oUpN69tE0LkOCA%3D&openid.identity=https%3A%2F%2Fwww.foogle.com%2Faccounts%2Fo8%2Fid%3Fid%3DAItOawlQCggILp_hoDD41sA1Fw2o9kEEnF9a9mk&openid.claimed_id=https%3A%2F%2Fwww.google.com%2Faccounts%2Fo8%2Fid%3Fid%3DAItOawlQCggILp_hoDD41sA1Fw2o9kEEnF9a9mk&openid.ns.ext1=http%3A%2F%2Fopenid.net%2Fsrv%2Fax%2F1.0&openid.ext1.mode=fetch_response&openid.ext1.type.firstname=http%3A%2F%2Faxschema.org%2FnamePerson%2Ffirst&openid.ext1.value.firstname=Christian&openid.ext1.type.email=http%3A%2F%2Faxschema.org%2Fcontact%2Femail&openid.ext1.value.email=christian.murray%40defold.se&openid.ext1.type.language=http%3A%2F%2Faxschema.org%2Fpref%2Flanguage&openid.ext1.value.language=sv&openid.ext1.type.lastname=http%3A%2F%2Faxschema.org%2FnamePerson%2Flast&openid.ext1.value.lastname=Murray";

    @Test
    public void testCreateAssociation() throws Exception {
        Association association = Association.create(testAssociationResponse);

        assertEquals("http://specs.openid.net/auth/2.0", association.get(Association.Key.ns));
        assertEquals("no-encryption", association.get(Association.Key.session_type));
        assertEquals("HMAC-SHA1", association.get(Association.Key.assoc_type));
        assertEquals("AOQobUfFOaDc6zFQ4PaGqJt5oNLes__P0CFci9gOxzHLqHX29hDFWjpD",
                association.get(Association.Key.assoc_handle));
        assertEquals("46800", association.get(Association.Key.expires_in));
        assertEquals("JdeP/doei+dloEhISpOoPHD1mIk=", association.get(Association.Key.mac_key));
        assertArrayEquals(Base64.decodeBase64("JdeP/doei+dloEhISpOoPHD1mIk="), association.getKey());
    }

    @Test
    public void testGetAuthenticationUrl() throws Exception {
        OpenID openId = new OpenID();
        IEndPointResolver resolver = mock(IEndPointResolver.class);
        openId.setEndPointResolver(resolver);

        IAssociator associator = mock(IAssociator.class);
        when(associator.associate(any(EndPoint.class))).thenReturn(
                Association.create(testAssociationResponse));
        openId.setAssociator(associator);

        long expires_in = 3600;
        when(resolver.resolve("google")).thenReturn(
                new EndPoint(new URL("http://example.com"), expires_in));

        openId.getAuthenticationUrl("google", "http://localhost", "http://localhost/auth");

        verify(resolver, times(1)).resolve("google");
        verify(associator, times(1)).associate(any(EndPoint.class));
    }

    @Test
    public void testEndPointCache1() throws Exception {
        // Verify that the end-point is expired
        OpenID openId = new OpenID();
        IEndPointResolver resolver = mock(IEndPointResolver.class);
        openId.setEndPointResolver(resolver);

        IAssociator associator = mock(IAssociator.class);
        when(associator.associate(any(EndPoint.class))).thenReturn(
                Association.create(testAssociationResponse));
        openId.setAssociator(associator);

        long expires_in = 1;
        when(resolver.resolve("google")).thenReturn(
                new EndPoint(new URL("http://example.com"), expires_in));

        openId.getAuthenticationUrl("google", "http://localhost", "http://localhost/auth");
        Thread.sleep(1000);
        openId.getAuthenticationUrl("google", "http://localhost", "http://localhost/auth");

        verify(resolver, times(2)).resolve("google");
        verify(associator, times(1)).associate(any(EndPoint.class));
    }

    @Test
    public void testEndPointCache2() throws Exception {
        // Verify that the end-point is reused
        OpenID openId = new OpenID();
        IEndPointResolver resolver = mock(IEndPointResolver.class);
        openId.setEndPointResolver(resolver);

        IAssociator associator = mock(IAssociator.class);
        when(associator.associate(any(EndPoint.class))).thenReturn(
                Association.create(testAssociationResponse));
        openId.setAssociator(associator);

        long expires_in = 3600;
        when(resolver.resolve("google")).thenReturn(
                new EndPoint(new URL("http://example.com"), expires_in));

        openId.getAuthenticationUrl("google", "http://localhost", "http://localhost/auth");
        Thread.sleep(1000);
        openId.getAuthenticationUrl("google", "http://localhost", "http://localhost/auth");

        verify(resolver, times(1)).resolve("google");
        verify(associator, times(1)).associate(any(EndPoint.class));
    }

    @Test
    public void testAssociationDoesntExpires() throws Exception {
        // Verify that association doesn't expire
        OpenID openId = new OpenID();
        IEndPointResolver resolver = mock(IEndPointResolver.class);
        openId.setEndPointResolver(resolver);

        IAssociator associator = mock(IAssociator.class);
        when(associator.associate(any(EndPoint.class))).thenReturn(
                Association.create(testAssociationResponse));
        openId.setAssociator(associator);

        long expires_in = 3600;
        when(resolver.resolve("google")).thenReturn(
                new EndPoint(new URL("http://example.com"), expires_in));

        openId.getAuthenticationUrl("google", "http://localhost", "http://localhost/auth");
        openId.getAuthenticationUrl("google", "http://localhost", "http://localhost/auth");

        verify(resolver, times(1)).resolve("google");
        verify(associator, times(1)).associate(any(EndPoint.class));
    }

    @Test
    public void testAssociationDoesExpires() throws Exception {
        // Verify that association does expire
        OpenID openId = new OpenID();
        IEndPointResolver resolver = mock(IEndPointResolver.class);
        openId.setEndPointResolver(resolver);

        IAssociator associator = mock(IAssociator.class);
        when(associator.associate(any(EndPoint.class))).thenReturn(
                Association.create(testAssociationResponseExpiresIn1));
        openId.setAssociator(associator);

        long expires_in = 3600;
        when(resolver.resolve("google")).thenReturn(
                new EndPoint(new URL("http://example.com"), expires_in));

        openId.getAuthenticationUrl("google", "http://localhost", "http://localhost/auth");
        Thread.sleep(1100);
        openId.getAuthenticationUrl("google", "http://localhost", "http://localhost/auth");

        verify(resolver, times(1)).resolve("google");
        verify(associator, times(2)).associate(any(EndPoint.class));
    }


    static void verifyResponse(String response) throws Exception {
        OpenID openId = new OpenID();
        IEndPointResolver resolver = mock(IEndPointResolver.class);
        openId.setEndPointResolver(resolver);

        IAssociator associator = mock(IAssociator.class);
        when(associator.associate(any(EndPoint.class))).thenReturn(
                Association.create(testAssociationResponse));
        openId.setAssociator(associator);

        long expires_in = 3600;
        when(resolver.resolve("google")).thenReturn(
                new EndPoint(new URL("http://example.com"), expires_in));

        openId.getAuthenticationUrl("google", "http://localhost", "http://localhost/auth");
        openId.verify(new URI(response));
    }

    @Test
    public void testVerify() throws Exception {
        verifyResponse(testAuthenticationResponse);
    }

    @Test(expected = OpenIDException.class)
    public void testVerifyTamperEmail() throws Exception {
        verifyResponse(testAuthenticationResponseTamperEmail);
    }

    @Test(expected = OpenIDException.class)
    public void testVerifyTamperIdentity() throws Exception {
        verifyResponse(testAuthenticationResponseTamperIdentity);
    }
}
