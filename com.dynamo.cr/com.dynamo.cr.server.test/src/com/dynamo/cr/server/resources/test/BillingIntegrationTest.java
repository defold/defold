package com.dynamo.cr.server.resources.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import java.io.File;
import java.net.URI;
import java.util.Properties;

import javax.inject.Singleton;
import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.UriBuilder;

import org.codehaus.jackson.JsonNode;
import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.eclipse.persistence.jpa.osgi.PersistenceProvider;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.dynamo.cr.common.providers.JsonProviders;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.proto.Config.BillingProduct;
import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.protocol.proto.Protocol.UserSubscriptionInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserSubscriptionState;
import com.dynamo.cr.server.ConfigurationProvider;
import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.billing.ChargifyService;
import com.dynamo.cr.server.billing.IBillingProvider;
import com.dynamo.cr.server.billing.test.ChargifySimulator;
import com.dynamo.cr.server.mail.IMailProcessor;
import com.dynamo.cr.server.mail.IMailer;
import com.dynamo.cr.server.mail.MailProcessor;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.model.UserSubscription;
import com.dynamo.cr.server.test.Util;
import com.dynamo.cr.server.util.ChargifyUtil;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.name.Names;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.UniformInterfaceException;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;
import com.sun.jersey.api.representation.Form;

public class BillingIntegrationTest {

    int port = 6500;
    String joeEmail = "joe@foo.com";
    String joePasswd = "secret2";
    User joeUser;
    DefaultClientConfig clientConfig;
    WebResource joeUsersWebResource;
    WebResource webhookResource;
    ChargifySimulator chargifySimulator;

    static Server server;
    static Module module;
    static IMailer mailer;
    static EntityManagerFactory emf;
    static IBillingProvider billingProvider;
    static BillingProduct freeProduct;
    static BillingProduct smallProduct;

    static class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(String.class).annotatedWith(Names.named("configurationFile"))
                    .toInstance("test_data/crepo_test.config");
            bind(Configuration.class).toProvider(ConfigurationProvider.class).in(Singleton.class);
            bind(IMailProcessor.class).to(MailProcessor.class).in(Singleton.class);
            bind(IMailer.class).toInstance(mailer);
            bind(IBillingProvider.class).to(ChargifyService.class).in(Singleton.class);

            Properties props = new Properties();
            props.put(PersistenceUnitProperties.CLASSLOADER, this.getClass().getClassLoader());
            EntityManagerFactory emf = new PersistenceProvider().createEntityManagerFactory("unit-test", props);
            bind(EntityManagerFactory.class).toInstance(emf);
        }
    }

    @BeforeClass
    public static void setUpClass() throws Exception {
        ClassLoader cl = Thread.currentThread().getContextClassLoader();
        cl.loadClass("org.apache.derby.jdbc.EmbeddedDriver");

        // "drop-and-create-tables" can't handle model changes correctly. We
        // need to drop all tables first.
        // Eclipse-link only drops tables currently specified. When the model
        // change the table set also change.
        File tmp_testdb = new File("tmp/testdb");
        if (tmp_testdb.exists()) {
            Util.dropAllTables();
        }

        mailer = mock(IMailer.class);

        module = new Module();
        Injector injector = Guice.createInjector(module);
        server = injector.getInstance(Server.class);
        emf = server.getEntityManagerFactory();

        freeProduct = server.getProductByHandle("free");
        smallProduct = server.getProductByHandle("small");
    }

    @AfterClass
    public static void tearDownClass() throws Exception {
        server.stop();
    }

    @Before
    public void setUp() throws Exception {
        // Clear all tables as we keep the database over tests
        Util.clearAllTables();

        EntityManager em = emf.createEntityManager();
        em.getTransaction().begin();

        joeUser = new User();
        joeUser.setEmail(joeEmail);
        joeUser.setFirstName("undefined");
        joeUser.setLastName("undefined");
        joeUser.setPassword(joePasswd);
        joeUser.setRole(Role.USER);
        em.persist(joeUser);

        em.getTransaction().commit();

        clientConfig = new DefaultClientConfig();
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyWriter.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        Client client = Client.create(clientConfig);

        URI uri = UriBuilder.fromUri(String.format("http://localhost/chargify")).port(port).build();
        this.webhookResource = client.resource(uri);

        client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        uri = UriBuilder.fromUri(String.format("http://localhost/users")).port(port).build();
        joeUsersWebResource = client.resource(uri);

        Configuration configuration = server.getConfiguration();

        client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(configuration.getBillingApiKey(), "x"));
        uri = UriBuilder.fromUri(configuration.getBillingApiUrl()).build();
        WebResource chargifyResource = client.resource(uri);

        chargifySimulator = new ChargifySimulator(chargifyResource, webhookResource,
                configuration.getBillingSharedKey());
        billingProvider = new ChargifyService(configuration);
    }

    private UserSubscriptionInfo getSubscription() {
        try {
            return joeUsersWebResource
                    .path(String.format("/%d/subscription", joeUser.getId()))
                    .accept(MediaType.APPLICATION_JSON_TYPE)
                    .type(MediaType.APPLICATION_JSON_TYPE)
                    .get(UserSubscriptionInfo.class);
        } catch (UniformInterfaceException e) {
            if (e.getResponse().getStatus() == 404) {
                return null;
            } else {
                throw e;
            }
        }
    }

    private void createUserSubscription(JsonNode root) {
        JsonNode subscription = root.get("subscription");
        int subscriptionId = subscription.get("id").getIntValue();
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("external_id", Integer.toString(subscriptionId))
                .post(ClientResponse.class);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());
    }

    @Test
    public void testSignUp() throws Exception {
        // Simulate hosted sign-up page
        JsonNode root = this.chargifySimulator.signUpUser(smallProduct, joeUser, "1", false);
        assertTrue(root != null);

        createUserSubscription(root);

        // Verify active
        UserSubscriptionInfo s = getSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, s.getState());

        // Webhooks
        assertTrue(this.chargifySimulator.sendWebhooks());

        // Verify active
        s = getSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, s.getState());
    }

    @Test
    public void testSignUpFailure() throws Exception {
        // Simulate hosted sign-up page, failure by credit card
        JsonNode subscription = this.chargifySimulator.signUpUser(smallProduct, joeUser, "2", false);
        assertTrue(subscription == null);
    }

    @Test
    public void testSignUpLaterFailure() throws Exception {
        // Simulate hosted sign-up page, failure by credit card
        JsonNode root = this.chargifySimulator.signUpUser(smallProduct, joeUser, "1", true);
        assertTrue(root != null);

        createUserSubscription(root);

        // Verify active
        UserSubscriptionInfo s = getSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, s.getState());

        // Webhooks
        assertTrue(this.chargifySimulator.sendWebhooks());

        // Verify canceled
        s = getSubscription();
        assertEquals(UserSubscriptionState.CANCELED, s.getState());
    }

    @Test
    public void testRenewalFailureReactivate() throws Exception {
        // Simulate hosted sign-up page
        JsonNode root = this.chargifySimulator.signUpUser(smallProduct, joeUser, "1", false);
        assertTrue(root != null);

        createUserSubscription(root);

        // Verify active
        UserSubscriptionInfo s = getSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, s.getState());

        // Webhooks
        assertTrue(this.chargifySimulator.sendWebhooks());

        // Verify active
        s = getSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, s.getState());

        int subscriptionId = root.get("subscription").get("id").getIntValue();
        Form f = new Form();
        f.add("payload[subscription][id]", Integer.toString(subscriptionId));
        ClientResponse response = ChargifyUtil.fakeWebhook(this.webhookResource, "renewal_failure", f, true, server
                .getConfiguration()
                .getBillingSharedKey());
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());
        // Also let chargify update its status
        UserSubscription subscription = new UserSubscription();
        subscription.setExternalId((long) subscriptionId);
        billingProvider.cancelSubscription(subscription);

        // Verify canceled
        s = getSubscription();
        assertEquals(UserSubscriptionState.CANCELED, s.getState());

        // Reactivate
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("state", "ACTIVE").put(ClientResponse.class);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        // Verify pending
        s = getSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, s.getState());

        // Webhook it
        f = new Form();
        f.add("payload[subscription][id]", Integer.toString(subscriptionId));
        f.add("payload[subscription][state]", "active");
        response = ChargifyUtil.fakeWebhook(this.webhookResource, "subscription_state_change", f, true, server
                .getConfiguration()
                .getBillingSharedKey());
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Verify active
        s = getSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, s.getState());
    }

    @Test
    public void testRenewalFailureTerminate() throws Exception {
        // Simulate hosted sign-up page
        JsonNode root = this.chargifySimulator.signUpUser(smallProduct, joeUser, "1", false);
        assertTrue(root != null);

        createUserSubscription(root);

        // Verify active
        UserSubscriptionInfo s = getSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, s.getState());

        // Webhooks
        assertTrue(this.chargifySimulator.sendWebhooks());

        // Verify active
        s = getSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, s.getState());

        Form f = new Form();
        f.add("payload[subscription][id]", Integer.toString(root.get("subscription").get("id").getIntValue()));
        ClientResponse response = ChargifyUtil.fakeWebhook(this.webhookResource, "renewal_failure", f, true, server
                .getConfiguration()
                .getBillingSharedKey());
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Verify canceled
        s = getSubscription();
        assertEquals(UserSubscriptionState.CANCELED, s.getState());

        // Terminate
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .delete(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Verify free product subscription
        s = getSubscription();
        assertEquals(freeProduct.getId(), s.getProduct().getId());
    }

    @Test
    public void testMigrate() throws Exception {
        // Simulate hosted sign-up page
        JsonNode root = this.chargifySimulator.signUpUser(smallProduct, joeUser, "1", false);
        assertTrue(root != null);

        createUserSubscription(root);

        // Verify active
        UserSubscriptionInfo s = getSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, s.getState());

        // Webhooks
        assertTrue(this.chargifySimulator.sendWebhooks());

        // Verify active
        s = getSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, s.getState());

        // Migrate
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(freeProduct.getId()))
                .put(ClientResponse.class);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        // Verify migration
        s = getSubscription();
        assertEquals(freeProduct.getId(), s.getProduct().getId());
    }
}
