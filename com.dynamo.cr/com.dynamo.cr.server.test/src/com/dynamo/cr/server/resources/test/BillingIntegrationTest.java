package com.dynamo.cr.server.resources.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.net.URI;

import javax.persistence.EntityManager;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.UriBuilder;

import org.codehaus.jackson.JsonNode;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.common.providers.JsonProviders;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.protocol.proto.Protocol.UserSubscriptionInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserSubscriptionState;
import com.dynamo.cr.server.billing.test.ChargifySimulator;
import com.dynamo.cr.server.model.Product;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.util.ChargifyUtil;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.UniformInterfaceException;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;
import com.sun.jersey.api.representation.Form;

public class BillingIntegrationTest extends AbstractResourceTest {

    int port = 6500;
    String joeEmail = "joe@foo.com";
    String joePasswd = "secret2";
    User joeUser;
    Product freeProduct;
    Product smallProduct;
    DefaultClientConfig clientConfig;
    WebResource joeUsersWebResource;
    WebResource webhookResource;
    ChargifySimulator chargifySimulator;

    @Before
    public void setUp() throws Exception {
        setupUpTest();

        EntityManager em = emf.createEntityManager();
        em.getTransaction().begin();

        joeUser = new User();
        joeUser.setEmail(joeEmail);
        joeUser.setFirstName("undefined");
        joeUser.setLastName("undefined");
        joeUser.setPassword(joePasswd);
        joeUser.setRole(Role.USER);
        em.persist(joeUser);

        freeProduct = new Product();
        freeProduct.setName("Free");
        freeProduct.setHandle("free");
        freeProduct.setMaxMemberCount(1);
        freeProduct.setDefault(true);
        em.persist(freeProduct);

        smallProduct = new Product();
        smallProduct.setName("Small");
        smallProduct.setHandle("small");
        smallProduct.setMaxMemberCount(-1);
        smallProduct.setDefault(false);
        em.persist(smallProduct);

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

    @Test
    public void testSignUp() throws Exception {
        // Simulate hosted sign-up page
        JsonNode subscription = this.chargifySimulator.signUpUser(smallProduct, joeUser, "1", false);
        assertTrue(subscription != null);

        // Create subscription
        int subscriptionId = subscription.get("subscription").get("id").getIntValue();
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(smallProduct.getId()))
                .queryParam("external_id", Integer.toString(subscriptionId))
                .post(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Verify pending
        UserSubscriptionInfo s = getSubscription();
        assertEquals(UserSubscriptionState.PENDING, s.getState());

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
        JsonNode subscription = this.chargifySimulator.signUpUser(smallProduct, joeUser, "1", true);
        assertTrue(subscription != null);

        // Create subscription
        int subscriptionId = subscription.get("subscription").get("id").getIntValue();
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(smallProduct.getId()))
                .queryParam("external_id", Integer.toString(subscriptionId))
                .post(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Verify pending
        UserSubscriptionInfo s = getSubscription();
        assertEquals(UserSubscriptionState.PENDING, s.getState());

        // Webhooks
        assertTrue(this.chargifySimulator.sendWebhooks());

        // Verify canceled
        s = getSubscription();
        assertEquals(UserSubscriptionState.CANCELED, s.getState());
    }

    @Test
    public void testRenewalFailureReactivate() throws Exception {
        // Simulate hosted sign-up page
        JsonNode subscription = this.chargifySimulator.signUpUser(smallProduct, joeUser, "1", false);
        assertTrue(subscription != null);

        // Create subscription
        int subscriptionId = subscription.get("subscription").get("id").getIntValue();
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(smallProduct.getId()))
                .queryParam("external_id", Integer.toString(subscriptionId))
                .post(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Verify pending
        UserSubscriptionInfo s = getSubscription();
        assertEquals(UserSubscriptionState.PENDING, s.getState());

        // Webhooks
        assertTrue(this.chargifySimulator.sendWebhooks());

        // Verify active
        s = getSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, s.getState());

        Form f = new Form();
        f.add("payload[subscription][id]", Integer.toString(subscriptionId));
        response = ChargifyUtil.fakeWebhook(this.webhookResource, "renewal_failure", f, true, server.getConfiguration()
                .getBillingSharedKey());
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Verify canceled
        s = getSubscription();
        assertEquals(UserSubscriptionState.CANCELED, s.getState());

        // Reactivate
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(smallProduct.getId()))
                .queryParam("state", "ACTIVE").put(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Verify pending
        s = getSubscription();
        assertEquals(UserSubscriptionState.PENDING, s.getState());

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
        JsonNode subscription = this.chargifySimulator.signUpUser(smallProduct, joeUser, "1", false);
        assertTrue(subscription != null);

        // Create subscription
        int subscriptionId = subscription.get("subscription").get("id").getIntValue();
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(smallProduct.getId()))
                .queryParam("external_id", Integer.toString(subscriptionId))
                .post(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Verify pending
        UserSubscriptionInfo s = getSubscription();
        assertEquals(UserSubscriptionState.PENDING, s.getState());

        // Webhooks
        assertTrue(this.chargifySimulator.sendWebhooks());

        // Verify active
        s = getSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, s.getState());

        Form f = new Form();
        f.add("payload[subscription][id]", Integer.toString(subscriptionId));
        response = ChargifyUtil.fakeWebhook(this.webhookResource, "renewal_failure", f, true, server.getConfiguration()
                .getBillingSharedKey());
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Verify canceled
        s = getSubscription();
        assertEquals(UserSubscriptionState.CANCELED, s.getState());

        // Terminate
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .delete(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Verify none
        s = getSubscription();
        assertEquals(null, s);
    }

    @Test
    public void testMigrate() throws Exception {
        // Simulate hosted sign-up page
        JsonNode subscription = this.chargifySimulator.signUpUser(smallProduct, joeUser, "1", false);
        assertTrue(subscription != null);

        // Create subscription
        int subscriptionId = subscription.get("subscription").get("id").getIntValue();
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(smallProduct.getId()))
                .queryParam("external_id", Integer.toString(subscriptionId))
                .post(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Verify pending
        UserSubscriptionInfo s = getSubscription();
        assertEquals(UserSubscriptionState.PENDING, s.getState());

        // Webhooks
        assertTrue(this.chargifySimulator.sendWebhooks());

        // Verify active
        s = getSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, s.getState());

        // Migrate
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(freeProduct.getId()))
                .queryParam("state", s.getState().toString())
                .put(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Verify migration
        s = getSubscription();
        assertEquals(freeProduct.getId().longValue(), s.getProduct().getId());
    }
}
