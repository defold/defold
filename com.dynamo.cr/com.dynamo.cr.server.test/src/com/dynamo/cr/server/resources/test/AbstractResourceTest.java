package com.dynamo.cr.server.resources.test;

import static org.mockito.Mockito.mock;

import java.io.File;
import java.net.URI;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;

import javax.inject.Singleton;
import javax.mail.MessagingException;
import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.ws.rs.core.UriBuilder;

import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.eclipse.persistence.jpa.PersistenceProvider;
import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import com.dynamo.cr.common.providers.JsonProviders;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.proto.Config.BillingProduct;
import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.ConfigurationProvider;
import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.billing.IBillingProvider;
import com.dynamo.cr.server.mail.EMail;
import com.dynamo.cr.server.mail.IMailProcessor;
import com.dynamo.cr.server.mail.IMailer;
import com.dynamo.cr.server.mail.MailProcessor;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.model.UserSubscription;
import com.dynamo.cr.server.model.UserSubscription.CreditCard;
import com.dynamo.cr.server.model.UserSubscription.State;
import com.dynamo.cr.server.test.Util;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.name.Names;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;

public class AbstractResourceTest {

    static Server server;
    static Module module;
    static TestMailer mailer;
    static EntityManagerFactory emf;
    static IBillingProvider billingProvider;
    static BillingProduct freeProduct;
    static BillingProduct smallProduct;
    static Injector injector;

    static class TestMailer implements IMailer {
        List<EMail> emails = new ArrayList<EMail>();
        @Override
        public void send(EMail email) throws MessagingException {
            emails.add(email);
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

        billingProvider = mock(IBillingProvider.class);

        module = new Module(mailer);
        injector = Guice.createInjector(module);
        server = injector.getInstance(Server.class);
        emf = server.getEntityManagerFactory();

        freeProduct = server.getProductByHandle("free");
        smallProduct = server.getProductByHandle("small");

        Mockito.doAnswer(new Answer<UserSubscription>() {
            @Override
            public UserSubscription answer(InvocationOnMock invocation) throws Throwable {
                UserSubscription subscription = new UserSubscription();
                subscription.setCreditCard(new CreditCard("1", 1, 2020));
                subscription.setExternalId((Long)invocation.getArguments()[0]);
                subscription.setExternalCustomerId(1l);
                subscription.setProductId((long) freeProduct.getId());
                subscription.setState(State.ACTIVE);
                return subscription;
            }
        }).when(billingProvider).getSubscription(Mockito.anyLong());
    }

    @AfterClass
    public static void tearDownClass() throws Exception {
        server.stop();
    }

    void setupUpTest() throws Exception {
        // Clear all tables as we keep the database over tests
        Util.clearAllTables();
        mailer.emails.clear();
    }

    static class Module extends AbstractModule {
        IMailer mailer;

        public Module() {
            mailer = mock(IMailer.class);
        }

        public Module(IMailer mailer) {
            this.mailer = mailer;
        }

        @Override
        protected void configure() {
            bind(String.class).annotatedWith(Names.named("configurationFile")).toInstance("test_data/crepo_test.config");
            bind(Configuration.class).toProvider(ConfigurationProvider.class).in(Singleton.class);
            bind(IMailProcessor.class).to(MailProcessor.class).in(Singleton.class);
            bind(IMailer.class).toInstance(mailer);
            bind(IBillingProvider.class).toInstance(billingProvider);

            Properties props = new Properties();
            props.put(PersistenceUnitProperties.CLASSLOADER, this.getClass().getClassLoader());
            EntityManagerFactory emf = new PersistenceProvider().createEntityManagerFactory("unit-test", props);
            bind(EntityManagerFactory.class).toInstance(emf);
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

        URI uri = UriBuilder.fromUri(String.format(baseURI)).port(port).build();
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

        URI uri = UriBuilder.fromUri(String.format(baseURI)).port(port).build();
        return client.resource(uri);
    }

}

