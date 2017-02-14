package com.dynamo.cr.server.clients.magazine;

import com.sun.jersey.api.client.Client;
import com.sun.jersey.multipart.MultiPart;
import com.sun.jersey.multipart.file.StreamDataBodyPart;

import javax.ws.rs.core.MediaType;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.interfaces.RSAPrivateKey;
import java.time.Duration;
import java.time.Instant;
import java.time.temporal.ChronoUnit;

public class MagazineClient {
    private static final Duration READ_EXPIRATION = Duration.of(5, ChronoUnit.HOURS);
    private static final Duration WRITE_EXPIRATION = Duration.of(10, ChronoUnit.MINUTES);

    private final String magazineServiceUrl;

    private final JwtFactory jwtFactory;
    private final Client client = Client.create();

    public MagazineClient(String magazineServiceUrl) throws Exception {
        RSAPrivateKey privateKey = RSAKeyReader.readPrivateKey("private_key.der");
        jwtFactory = new JwtFactory(privateKey);
        this.magazineServiceUrl = magazineServiceUrl;
    }

    public void put(String token, String relativePath, Path path) throws IOException {

        if (Files.isDirectory(path)) {
            System.out.println("Skip directory: " + path);
            return;
        }

        InputStream inputStream = Files.newInputStream(path);
        String filename = path.getFileName().toString();

        put(token, relativePath, inputStream, filename);
    }

    public void put(String token, String relativePath, InputStream inputStream, String filename) {
        MultiPart multiPart = new MultiPart(MediaType.MULTIPART_FORM_DATA_TYPE);
        StreamDataBodyPart streamDataBodyPart = new StreamDataBodyPart("file", inputStream, filename);
        multiPart.bodyPart(streamDataBodyPart);

        client.resource(magazineServiceUrl)
                .path(token)
                .path(relativePath)
                .type(MediaType.MULTIPART_FORM_DATA_TYPE)
                .put(multiPart);
    }

    public String createWriteToken(String user, String contextPath) {
        return jwtFactory.create(user, Instant.now().plus(WRITE_EXPIRATION), contextPath, true);
    }

    public String createReadUrl(String resource) {
        String jwt = jwtFactory.create("", Instant.now().plus(READ_EXPIRATION), resource, false);
        return magazineServiceUrl + "/" + jwt;
    }

    public void delete(String userEmail, String resource) {
        String writeToken = createWriteToken(userEmail, resource);

        client.resource(magazineServiceUrl)
                .path(writeToken)
                .delete();
    }
}
