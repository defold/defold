package com.dynamo.cr.server.resources.test;

import static org.junit.Assert.assertEquals;

import java.net.URI;

import javax.persistence.EntityManager;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.UriBuilder;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.common.providers.JsonProviders;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.protocol.proto.Protocol.UserSubscriptionInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserSubscriptionState;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.util.ChargifyUtil;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;
import com.sun.jersey.api.representation.Form;

public class ChargifyResourceTest extends AbstractResourceTest {

    int port = 6500;
    Long externalId = 2l;
    String joeEmail = "joe@foo.com";
    String joePasswd = "secret2";
    User joeUser;
    DefaultClientConfig clientConfig;
    WebResource joeUsersWebResource;
    WebResource chargifyResource;
    WebResource productsResource;

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

        em.getTransaction().commit();

        clientConfig = new DefaultClientConfig();
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyWriter.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        Client client = Client.create(clientConfig);

        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/users")).port(port).build();
        joeUsersWebResource = client.resource(uri);

        client = Client.create(clientConfig);
        uri = UriBuilder.fromUri(String.format("http://localhost/chargify")).port(port).build();
        chargifyResource = client.resource(uri);

        client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        productsResource = client.resource(uri);
    }

    private ClientResponse post(String event, Form f, boolean sign) throws Exception {
        return ChargifyUtil.fakeWebhook(this.chargifyResource, event, f, sign, server.getConfiguration()
                .getBillingSharedKey());
    }

    @Test
    public void testNoAccess() throws Exception {
        ClientResponse response = post(ChargifyUtil.SIGNUP_SUCCESS_WH, new Form(), false);
        assertEquals(ClientResponse.Status.FORBIDDEN.getStatusCode(), response.getClientResponseStatus()
                .getStatusCode());
    }

    @Test
    public void testUnsupportedHook() throws Exception {
        ClientResponse response = post("unsupported", new Form(), true);
        assertEquals(ClientResponse.Status.NO_CONTENT.getStatusCode(), response.getClientResponseStatus()
                .getStatusCode());
    }

    private UserSubscriptionInfo createUserSubscription() {
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("external_id", externalId.toString())
                .post(ClientResponse.class);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());
        // Retrieve it
        UserSubscriptionInfo subscriptionInfo = joeUsersWebResource
                .path(String.format("/%d/subscription", joeUser.getId())).accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(UserSubscriptionInfo.class);
        assertEquals(UserSubscriptionState.ACTIVE, subscriptionInfo.getState());
        return subscriptionInfo;
    }

    @Test
    public void testSubscriptionSuccess() throws Exception {
        createUserSubscription();

        // Activate through webhook
        Form f = new Form();
        f.add("payload[subscription][id]", externalId.toString());
        ClientResponse response = post(ChargifyUtil.SIGNUP_SUCCESS_WH, f, true);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Retrieve it
        UserSubscriptionInfo subscriptionInfo = joeUsersWebResource
                .path(String.format("/%d/subscription", joeUser.getId())).accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(UserSubscriptionInfo.class);
        assertEquals(UserSubscriptionState.ACTIVE, subscriptionInfo.getState());
    }

    @Test
    public void testSubscriptionFailure() throws Exception {
        UserSubscriptionInfo subscriptionInfo = createUserSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, subscriptionInfo.getState());

        // Cancel through webhook
        Form f = new Form();
        f.add("payload[subscription][id]", externalId.toString());
        ClientResponse response = post(ChargifyUtil.SIGNUP_FAILURE_WH, f, true);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Retrieve it
        subscriptionInfo = joeUsersWebResource
                .path(String.format("/%d/subscription", joeUser.getId()))
                .accept(MediaType.APPLICATION_JSON_TYPE).type(MediaType.APPLICATION_JSON_TYPE)
                .get(UserSubscriptionInfo.class);
        assertEquals(UserSubscriptionState.CANCELED, subscriptionInfo.getState());
    }

    @Test
    public void testRenewalFailure() throws Exception {
        UserSubscriptionInfo subscriptionInfo = createUserSubscription();
        assertEquals(UserSubscriptionState.ACTIVE, subscriptionInfo.getState());

        final String MESSAGE = "TEST";

        // Cancel through webhook
        Form f = new Form();
        f.add("payload[subscription][id]", externalId.toString());
        f.add("payload[subscription][state]", "canceled");
        f.add("payload[subscription][previous_state]", "active");
        f.add("payload[subscription][cancellation_message]", MESSAGE);
        ClientResponse response = post(ChargifyUtil.SUBSCRIPTION_STATE_CHANGE_WH, f, true);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Retrieve it
        subscriptionInfo = joeUsersWebResource
                .path(String.format("/%d/subscription", joeUser.getId()))
                .accept(MediaType.APPLICATION_JSON_TYPE).type(MediaType.APPLICATION_JSON_TYPE)
                .get(UserSubscriptionInfo.class);
        assertEquals(UserSubscriptionState.CANCELED, subscriptionInfo.getState());
        assertEquals(MESSAGE, subscriptionInfo.getCancellationMessage());
    }

    @Test
    public void testMigration() throws Exception {
        UserSubscriptionInfo subscriptionInfo = createUserSubscription();

        assertEquals(freeProduct.getId(), subscriptionInfo.getProduct().getId());

        // Migrate through webhook
        Form f = new Form();
        f.add("payload[subscription][id]", externalId.toString());
        f.add("payload[subscription][state]", "active");
        f.add("payload[subscription][product][handle]", smallProduct.getHandle());
        f.add("payload[subscription][previous_product][handle]", freeProduct.getHandle());
        ClientResponse response = post(ChargifyUtil.SUBSCRIPTION_PRODUCT_CHANGE_WH, f, true);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Retrieve it
        subscriptionInfo = joeUsersWebResource
                .path(String.format("/%d/subscription", joeUser.getId()))
                .accept(MediaType.APPLICATION_JSON_TYPE).type(MediaType.APPLICATION_JSON_TYPE)
                .get(UserSubscriptionInfo.class);
        assertEquals(smallProduct.getId(), subscriptionInfo.getProduct().getId());
    }
}
