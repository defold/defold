package com.dynamo.cr.server.billing.test;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;

import java.io.File;
import java.io.IOException;
import java.net.URI;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;

import javax.inject.Singleton;
import javax.mail.MessagingException;
import javax.persistence.EntityManagerFactory;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.UriBuilder;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.JsonProcessingException;
import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ObjectNode;
import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.eclipse.persistence.jpa.PersistenceProvider;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.dynamo.cr.proto.Config.BillingProduct;
import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.ConfigurationProvider;
import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.billing.ChargifyService;
import com.dynamo.cr.server.billing.IBillingProvider;
import com.dynamo.cr.server.mail.EMail;
import com.dynamo.cr.server.mail.IMailProcessor;
import com.dynamo.cr.server.mail.IMailer;
import com.dynamo.cr.server.mail.MailProcessor;
import com.dynamo.cr.server.model.UserSubscription;
import com.dynamo.cr.server.model.UserSubscription.CreditCard;
import com.dynamo.cr.server.model.UserSubscription.State;
import com.dynamo.cr.server.test.Util;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.name.Names;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;

public class ChargifyServiceTest {

    private static final String CUSTOMER_REFERENCE = "test_user";

    static Server server;
    static Module module;
    static TestMailer mailer;
    static EntityManagerFactory emf;
    static IBillingProvider billingProvider;
    static Long subscriptionId;
    static Long customerId;
    static Long productId;

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

        module = new Module(mailer);
        Injector injector = Guice.createInjector(module);
        server = injector.getInstance(Server.class);
        emf = server.getEntityManagerFactory();
        billingProvider = injector.getInstance(IBillingProvider.class);

        verifyBootstrapData();
    }

    @AfterClass
    public static void tearDownClass() throws Exception {
        server.stop();
    }

    @Before
    public void setUp() throws Exception {
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
            bind(IBillingProvider.class).to(ChargifyService.class).in(Singleton.class);

            Properties props = new Properties();
            props.put(PersistenceUnitProperties.CLASSLOADER, this.getClass().getClassLoader());
            EntityManagerFactory emf = new PersistenceProvider().createEntityManagerFactory("unit-test", props);
            bind(EntityManagerFactory.class).toInstance(emf);
        }
    }

    private static void verifyBootstrapData() throws JsonProcessingException, IOException {
        Configuration configuration = server.getConfiguration();
        Client client = Client.create(new DefaultClientConfig());
        client.addFilter(new HTTPBasicAuthFilter(configuration.getBillingApiKey(), "x"));
        URI uri = UriBuilder.fromUri(configuration.getBillingApiUrl()).build();
        WebResource resource = client.resource(uri);

        ObjectMapper mapper = new ObjectMapper();
        ClientResponse response = resource.path("/customers/lookup.json").queryParam("reference", CUSTOMER_REFERENCE)
                .accept(MediaType.APPLICATION_JSON_TYPE).type(MediaType.APPLICATION_JSON_TYPE)
                .get(ClientResponse.class);
        if (response.getStatus() == ClientResponse.Status.NOT_FOUND.getStatusCode()) {
            ObjectNode root = mapper.createObjectNode();
            ObjectNode customer = mapper.createObjectNode();
            customer.put("first_name", "Testing");
            customer.put("last_name", "User");
            customer.put("email", "chargify-test-user@defold.se");
            customer.put("reference", CUSTOMER_REFERENCE);
            root.put("customer", customer);
            response = resource.path("/customers.json").accept(MediaType.APPLICATION_JSON_TYPE)
                    .type(MediaType.APPLICATION_JSON_TYPE).post(ClientResponse.class, root.toString());
            JsonNode c = mapper.readTree(response.getEntityInputStream());
            customerId = (long) c.get("customer").get("id").getIntValue();
            root = mapper.createObjectNode();
            ObjectNode subscription = mapper.createObjectNode();
            subscription.put("product_handle", "small");
            subscription.put("customer_id", Long.toString(customerId));
            ObjectNode cc = mapper.createObjectNode();
            cc.put("full_number", "1");
            cc.put("expiration_month", "12");
            cc.put("expiration_year", "2020");
            subscription.put("credit_card_attributes", cc);
            root.put("subscription", subscription);
            response = resource.path("/subscriptions.json").accept(MediaType.APPLICATION_JSON_TYPE)
                    .type(MediaType.APPLICATION_JSON_TYPE).post(ClientResponse.class, root.toString());
            JsonNode s = mapper.readTree(response.getEntityInputStream());
            subscriptionId = new Long(s.get("subscription").get("id").getIntValue());
            productId = (long) s.get("subscription").get("product").get("id").getIntValue();
        } else if (response.getStatus() == ClientResponse.Status.OK.getStatusCode()) {
            JsonNode root = mapper.readTree(response.getEntityInputStream());
            customerId = (long) root.get("customer").get("id").getIntValue();
            response = resource.path(String.format("/customers/%d/subscriptions.json", customerId))
                    .accept(MediaType.APPLICATION_JSON_TYPE).type(MediaType.APPLICATION_JSON_TYPE)
                    .get(ClientResponse.class);
            root = mapper.readTree(response.getEntityInputStream());
            JsonNode s = root.get(0).get("subscription");
            subscriptionId = new Long(s.get("id").getIntValue());
            productId = (long) s.get("product").get("id").getIntValue();
        }
    }

    @Test
    public void testMigrate() {
        UserSubscription subscription = new UserSubscription();
        subscription.setExternalId(subscriptionId);
        BillingProduct free = server.getProductByHandle("free");
        BillingProduct small = server.getProductByHandle("small");

        billingProvider.migrateSubscription(subscription, free);
        billingProvider.migrateSubscription(subscription, small);
    }

    @Test
    public void testCancelReactivate() {
        UserSubscription subscription = new UserSubscription();
        subscription.setExternalId(subscriptionId);

        billingProvider.cancelSubscription(subscription);
        billingProvider.reactivateSubscription(subscription);
    }

    @Test
    public void testRetrieve() {
        UserSubscription subscription = billingProvider.getSubscription(subscriptionId);
        CreditCard cc = subscription.getCreditCard();
        assertEquals("XXXX-XXXX-XXXX-1", cc.getMaskedNumber());
        assertEquals(12, cc.getExpirationMonth());
        assertEquals(2020, cc.getExpirationYear());
        assertEquals(subscriptionId, subscription.getExternalId());
        assertEquals(customerId, subscription.getExternalCustomerId());
        assertEquals(productId, subscription.getProductId());
        assertEquals(State.ACTIVE, subscription.getState());
    }

}

