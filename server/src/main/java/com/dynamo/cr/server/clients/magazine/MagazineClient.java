package com.dynamo.cr.server.clients.magazine;

import com.dynamo.cr.proto.Config;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.multipart.MultiPart;
import com.sun.jersey.multipart.file.StreamDataBodyPart;

import javax.inject.Inject;
import javax.ws.rs.core.MediaType;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.interfaces.RSAPrivateKey;
import java.time.Duration;
import java.time.Instant;
import java.time.temporal.ChronoUnit;

public class MagazineClient {
    private static final Duration READ_EXPIRATION = Duration.of(5, ChronoUnit.HOURS);
    private static final Duration WRITE_EXPIRATION = Duration.of(10, ChronoUnit.MINUTES);

    private final JwtFactory jwtFactory;
    private final Client client = Client.create();

    @Inject
    private Config.Configuration configuration;

    public MagazineClient() throws Exception {
        RSAPrivateKey privateKey = RSAKeyReader.readPrivateKey("private_key.der");
        jwtFactory = new JwtFactory(privateKey);
    }

    private String getMagazineServiceUrl() {
        return configuration.getMagazineServiceUrl();
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

        client.resource(getMagazineServiceUrl())
                .path(token)
                .path(relativePath)
                .type(MediaType.MULTIPART_FORM_DATA_TYPE)
                .put(multiPart);
    }

    public String createWriteToken(String user, String contextPath) {
        return jwtFactory.create(user, Instant.now().plus(WRITE_EXPIRATION), contextPath, true);
    }

    public String createReadUrl(String resource) {
        String fileName = Paths.get(resource).getFileName().toString();
        String path = Paths.get(resource).getParent().toString();
        String jwt = jwtFactory.create("", Instant.now().plus(READ_EXPIRATION), path, false);
        return getMagazineServiceUrl() + "/" + jwt + "/" + fileName;
    }

    public void delete(String userEmail, String resource) {
        String writeToken = createWriteToken(userEmail, resource);

        client.resource(getMagazineServiceUrl())
                .path(writeToken)
                .delete();
    }
}
