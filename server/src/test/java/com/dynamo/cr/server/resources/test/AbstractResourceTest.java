package com.dynamo.cr.server.resources.test;

import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.ConfigurationProvider;
import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.auth.AccessTokenAuthenticator;
import com.dynamo.cr.server.auth.AccessTokenStore;
import com.dynamo.cr.server.mail.EMail;
import com.dynamo.cr.server.mail.IMailProcessor;
import com.dynamo.cr.server.mail.IMailer;
import com.dynamo.cr.server.mail.MailProcessor;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.providers.JsonProviders;
import com.dynamo.cr.server.providers.ProtobufProviders;
import com.dynamo.cr.server.test.Util;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.name.Names;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;
import org.apache.commons.io.FileUtils;
import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.eclipse.persistence.jpa.PersistenceProvider;
import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.inject.Singleton;
import javax.mail.MessagingException;
import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.ws.rs.core.UriBuilder;
import java.io.File;
import java.io.IOException;
import java.net.URI;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;

import static org.junit.Assert.assertEquals;

public class AbstractResourceTest {

    private static final Logger LOGGER = LoggerFactory.getLogger(AbstractResourceTest.class);

    private static Injector injector;

    static Server server;
    static TestMailer mailer;
    static EntityManagerFactory emf;

    static class TestMailer implements IMailer {
        final List<EMail> emails = new ArrayList<>();

        @Override
        public void send(EMail email) throws MessagingException {
            emails.add(email);
        }

        List<EMail> getEmails() {
            return emails;
        }
    }

    @BeforeClass
    public static void setUpClass() throws Exception {
        ClassLoader cl = Thread.currentThread().getContextClassLoader();
        cl.loadClass("org.apache.derby.jdbc.EmbeddedDriver");

        // "drop-and-create-tables" can't handle model changes correctly. We need to drop all tables first.
        // Eclipse-link only drops tables currently specified. When the model change the table set also change.
        File tmp_testdb = new File("tmp/testdb");
        if (tmp_testdb.exists()) {
            Util.dropAllTables();
        }

        mailer = new TestMailer();

        Module module = new Module(mailer);
        injector = Guice.createInjector(module);
        server = injector.getInstance(Server.class);
        emf = server.getEntityManagerFactory();

        clearTemporaryFileArea();
    }

    @AfterClass
    public static void tearDownClass() {
        server.stop();
    }

    void setupUpTest() throws Exception {
        // Clear all tables as we keep the database over tests
        emf.getCache().evictAll();
        Util.clearAllTables();
        mailer.emails.clear();
    }

    private static class Module extends AbstractModule {
        final IMailer mailer;

        Module(IMailer mailer) {
            this.mailer = mailer;
        }

        @Override
        protected void configure() {
            System.setProperty(PersistenceUnitProperties.ECLIPSELINK_PERSISTENCE_XML, "META-INF/test_persistence.xml");
            bind(String.class).annotatedWith(Names.named("configurationFile")).toInstance("test_data/crepo_test.config");
            bind(Configuration.class).toProvider(ConfigurationProvider.class).in(Singleton.class);
            bind(IMailProcessor.class).to(MailProcessor.class).in(Singleton.class);
            bind(IMailer.class).toInstance(mailer);
            bind(AccessTokenStore.class);
            bind(AccessTokenAuthenticator.class);

            Properties props = new Properties();
            props.put(PersistenceUnitProperties.CLASSLOADER, this.getClass().getClassLoader());
            EntityManagerFactory emf = new PersistenceProvider().createEntityManagerFactory("unit-test", props);
            bind(EntityManagerFactory.class).toInstance(emf);
            bind(EntityManager.class).toInstance(emf.createEntityManager());
        }
    }

    User createUser(String email, String password, String firstName, String lastName, Role role) {
        EntityManager em = emf.createEntityManager();
        em.getTransaction().begin();
        User user = new User();
        user.setEmail(email);
        user.setFirstName(firstName);
        user.setLastName(lastName);
        user.setPassword(password);
        user.setRole(role);
        em.persist(user);
        em.getTransaction().commit();
        return user;
    }

    WebResource createResource(String baseURI, User user, String password) {
        DefaultClientConfig clientConfig = new DefaultClientConfig();
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyWriter.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(user.getEmail(), password));

        int port = injector.getInstance(Configuration.class).getServicePort();

        URI uri = UriBuilder.fromUri(baseURI).port(port).build();
        return client.resource(uri);
    }

    WebResource createBaseResource(User user, String password) {
        DefaultClientConfig clientConfig = new DefaultClientConfig();
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyWriter.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(user.getEmail(), password));

        int port = injector.getInstance(Configuration.class).getServicePort();

        URI uri = UriBuilder.fromUri("http://localhost/").port(port).build();
        return client.resource(uri);
    }

    WebResource createAnonymousResource(String baseURI) {
        DefaultClientConfig clientConfig = new DefaultClientConfig();
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyWriter.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        Client client = Client.create(clientConfig);

        int port = injector.getInstance(Configuration.class).getServicePort();

        URI uri = UriBuilder.fromUri(baseURI).port(port).build();
        return client.resource(uri);
    }

    void execCommand(String command, String arg) throws IOException {
        TestUtil.Result r = TestUtil.execCommand(new String[]{"/bin/bash", command, arg});
        if (r.exitValue != 0) {
            System.err.println(r.stdOut);
            System.err.println(r.stdErr);
        }
        assertEquals(0, r.exitValue);
    }

    static void clearTemporaryFileArea() {
        String cacheDirName = server.getConfiguration().getArchiveCacheRoot();
        File cacheDir = new File(cacheDirName);

        if (!cacheDir.isDirectory()) {
            return;
        }

        File[] files = cacheDir.listFiles();
        if (files != null) {
            for (File dirFile : files) {
                if (!dirFile.getName().startsWith(".")) {
                    try {
                        FileUtils.deleteDirectory(dirFile);
                    } catch (IOException | IllegalArgumentException e) {
                        LOGGER.warn("Could not remove temporary file directory: " + dirFile.getName());
                    }
                }
            }
        }
    }

    File getRepositoryDir(long projectId) {
        String repositoryRoot = server.getConfiguration().getRepositoryRoot();
        return new File(String.format("%s/%d", repositoryRoot, projectId));
    }
}

