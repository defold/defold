package com.dynamo.cr.server.clients.magazine;

import java.net.URL;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.KeyFactory;
import java.security.interfaces.RSAPrivateKey;
import java.security.interfaces.RSAPublicKey;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;

public class RSAKeyReader {

    /**
     * Reads a public RSA key in DER format from a resource on the classpath.
     *
     * @param resourceName Resource name on the classpath.
     * @return Public RSA key.
     * @throws Exception Throws exception if failing to read key from classpath.
     */
    public static RSAPublicKey readPublicKey(String resourceName) throws Exception {
        X509EncodedKeySpec spec = new X509EncodedKeySpec(getKeyFromFile(resourceName));
        KeyFactory kf = KeyFactory.getInstance("RSA");
        return (RSAPublicKey) kf.generatePublic(spec);
    }

    /**
     * Reads a private RSA key in DER format from a resource on the classpath.
     *
     * @param resourceName Resource name on the classpath.
     * @return Private RSA key.
     * @throws Exception Throws exception if failing to read key from classpath.
     */
    public static RSAPrivateKey readPrivateKey(String resourceName) throws Exception {
        PKCS8EncodedKeySpec spec = new PKCS8EncodedKeySpec(getKeyFromFile(resourceName));
        KeyFactory kf = KeyFactory.getInstance("RSA");
        return (RSAPrivateKey) kf.generatePrivate(spec);
    }

    private static byte[] getKeyFromFile(String resourceName) throws Exception {
        URL systemResource = ClassLoader.getSystemResource(resourceName);

        if (systemResource == null) {
            throw new Exception("Resource " + resourceName + "could not be found on classpath.");
        }

        return Files.readAllBytes(Paths.get(systemResource.toURI()));
    }
}
