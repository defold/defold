package com.dynamo.cr.server.clients.magazine;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
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
        ResourceLoader resourceLoader = new ResourceLoader();
        return resourceLoader.getBytesFromResource(resourceName);
    }

    private static class ResourceLoader {

        private byte[] getBytesFromResource(String resourceName) throws IOException {
            try (InputStream inputStream = getInputStream(resourceName)) {
                return getBytesFromStream(inputStream);
            }
        }

        InputStream getInputStream(String resourceName) throws IOException {
            final ClassLoader[] classLoaders = {
                    getClass().getClassLoader(),
                    RSAKeyReader.class.getClassLoader(),
                    Thread.currentThread().getContextClassLoader(),
                    ClassLoader.getSystemClassLoader()
            };
            resourceName = removePreSlash(resourceName);
            URL resource = null;
            for (ClassLoader classLoader : classLoaders) {
                resource = classLoader.getResource(resourceName);

                if (resource != null) {
                    break;
                }
                resource = classLoader.getResource("/" + resourceName);
                if (resource != null) {
                    break;
                }
            }
            if (resource == null) {
                throw new IOException("Could not find resource: " + resourceName);
            }
            return resource.openStream();
        }

        private static String removePreSlash(String resourceName) {
            if (resourceName.charAt(0) == '/') {
                return resourceName.substring(1);
            }
            return resourceName;
        }
    }

    static byte[] getBytesFromStream(InputStream inputStream) throws IOException {
        try (ByteArrayOutputStream outputStream = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[1024];
            int read;
            while ((read = inputStream.read(buffer)) != -1) {
                outputStream.write(buffer, 0, read);
            }
            return outputStream.toByteArray();
        } finally {
            if (inputStream != null) {
                inputStream.close();
            }
        }
    }
}
