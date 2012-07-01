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
import com.dynamo.cr.protocol.proto.Protocol.ProductInfoList;
import com.dynamo.cr.protocol.proto.Protocol.UserSubscriptionInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserSubscriptionState;
import com.dynamo.cr.server.model.Product;
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
    String joeEmail = "joe@foo.com";
    String joePasswd = "secret2";
    User joeUser;
    Product freeProduct;
    Product smallProduct;
    DefaultClientConfig clientConfig;
    WebResource joeUsersWebResource;
    WebResource chargifyResource;

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

        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/users")).port(port).build();
        joeUsersWebResource = client.resource(uri);

        client = Client.create(clientConfig);
        uri = UriBuilder.fromUri(String.format("http://localhost/chargify")).port(port).build();
        chargifyResource = client.resource(uri);
    }

    private ClientResponse post(String event, Form f, boolean sign) throws Exception {
        return ChargifyUtil.fakeWebhook(this.chargifyResource, event, f, sign, server.getConfiguration()
                .getBillingSharedKey());
    }

    @Test
    public void testNoAccess() throws Exception {
        ClientResponse response = post("signup_success", new Form(), false);
        assertEquals(ClientResponse.Status.FORBIDDEN.getStatusCode(), response.getClientResponseStatus()
                .getStatusCode());
    }

    @Test
    public void testUnsupportedHook() throws Exception {
        ClientResponse response = post("unsupported", new Form(), true);
        assertEquals(ClientResponse.Status.BAD_REQUEST.getStatusCode(), response.getClientResponseStatus()
                .getStatusCode());
    }

    private void createUserSubscription(Long productId, Long externalId, Long externalCustomerId) {
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", productId.toString())
                .queryParam("external_id", externalId.toString())
                .queryParam("external_customer_id", externalCustomerId.toString()).post(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());
    }

    @Test
    public void testSubscriptionSuccess() throws Exception {
        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        WebResource productsResource = client.resource(uri);
        ProductInfoList productInfoList = productsResource.accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class);

        Long externalId = 2l;
        Long externalCustomerId = 3l;

        createUserSubscription(productInfoList.getProducts(0).getId(), externalId, externalCustomerId);

        // Retrieve it
        UserSubscriptionInfo subscriptionInfo = joeUsersWebResource
                .path(String.format("/%d/subscription", joeUser.getId())).accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(UserSubscriptionInfo.class);
        assertEquals(UserSubscriptionState.PENDING, subscriptionInfo.getState());

        // Activate through webhook
        Form f = new Form();
        f.add("payload[subscription][id]", externalId.toString());
        ClientResponse response = post("signup_success", f, true);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Retrieve it
        subscriptionInfo = joeUsersWebResource
                .path(String.format("/%d/subscription", joeUser.getId())).accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(UserSubscriptionInfo.class);
        assertEquals(UserSubscriptionState.ACTIVE, subscriptionInfo.getState());
    }

    @Test
    public void testSubscriptionFailure() throws Exception {
        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        WebResource productsResource = client.resource(uri);
        ProductInfoList productInfoList = productsResource.type(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class);

        Long externalId = 2l;
        Long externalCustomerId = 3l;

        createUserSubscription(productInfoList.getProducts(0).getId(), externalId, externalCustomerId);

        // Retrieve it
        UserSubscriptionInfo subscriptionInfo = joeUsersWebResource
                .path(String.format("/%d/subscription", joeUser.getId())).accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(UserSubscriptionInfo.class);
        assertEquals(UserSubscriptionState.PENDING, subscriptionInfo.getState());

        // Activate through webhook
        Form f = new Form();
        f.add("payload[subscription][id]", externalId.toString());
        ClientResponse response = post("signup_failure", f, true);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Retrieve it
        subscriptionInfo = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .accept(MediaType.APPLICATION_JSON_TYPE).type(MediaType.APPLICATION_JSON_TYPE)
                .get(UserSubscriptionInfo.class);
        assertEquals(UserSubscriptionState.CANCELED, subscriptionInfo.getState());
    }

    @Test
    public void testRenewalFailure() throws Exception {
        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        WebResource productsResource = client.resource(uri);
        ProductInfoList productInfoList = productsResource.type(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class);

        Long externalId = 2l;
        Long externalCustomerId = 3l;

        createUserSubscription(productInfoList.getProducts(0).getId(), externalId, externalCustomerId);

        // Retrieve it
        UserSubscriptionInfo subscriptionInfo = joeUsersWebResource
                .path(String.format("/%d/subscription", joeUser.getId())).accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(UserSubscriptionInfo.class);
        assertEquals(UserSubscriptionState.PENDING, subscriptionInfo.getState());

        // Activate through webhook
        Form f = new Form();
        f.add("payload[subscription][id]", externalId.toString());
        ClientResponse response = post("renewal_failure", f, true);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Retrieve it
        subscriptionInfo = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .accept(MediaType.APPLICATION_JSON_TYPE).type(MediaType.APPLICATION_JSON_TYPE)
                .get(UserSubscriptionInfo.class);
        assertEquals(UserSubscriptionState.CANCELED, subscriptionInfo.getState());
    }
}

