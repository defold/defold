package com.dynamo.cr.server.resources.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;

import java.io.ByteArrayInputStream;
import java.net.URI;
import java.util.List;

import javax.persistence.EntityManager;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.UriBuilder;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ObjectNode;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mockito;

import com.dynamo.cr.client.filter.DefoldAuthFilter;
import com.dynamo.cr.common.providers.JsonProviders;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.protocol.proto.Protocol.LoginInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProductInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProductInfoList;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfoList;
import com.dynamo.cr.protocol.proto.Protocol.UserSubscriptionInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserSubscriptionState;
import com.dynamo.cr.server.model.Invitation;
import com.dynamo.cr.server.model.InvitationAccount;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.NewUser;
import com.dynamo.cr.server.model.Product;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.model.UserSubscription;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.ClientResponse.Status;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;

public class UsersResourceTest extends AbstractResourceTest {

    int port = 6500;
    String joeEmail = "joe@foo.com";
    String joePasswd = "secret2";
    String bobEmail = "bob@foo.com";
    String bobPasswd = "secret3";
    String adminEmail = "admin@foo.com";
    String adminPasswd = "secret";
    User joeUser;
    User adminUser;
    User bobUser;
    Product freeProduct;
    Product smallProduct;
    WebResource adminUsersWebResource;
    WebResource joeUsersWebResource;
    WebResource bobUsersWebResource;
    WebResource joeDefoldAuthUsersWebResource;
    DefaultClientConfig clientConfig;
    WebResource anonymousResource;

    @Before
    public void setUp() throws Exception {
        setupUpTest();

        EntityManager em = emf.createEntityManager();
        em.getTransaction().begin();
        adminUser = new User();
        adminUser.setEmail(adminEmail);
        adminUser.setFirstName("undefined");
        adminUser.setLastName("undefined");
        adminUser.setPassword(adminPasswd);
        adminUser.setRole(Role.ADMIN);
        em.persist(adminUser);

        joeUser = new User();
        joeUser.setEmail(joeEmail);
        joeUser.setFirstName("undefined");
        joeUser.setLastName("undefined");
        joeUser.setPassword(joePasswd);
        joeUser.setRole(Role.USER);
        em.persist(joeUser);
        InvitationAccount joeAccount = new InvitationAccount();
        joeAccount.setUser(joeUser);
        joeAccount.setOriginalCount(1);
        joeAccount.setCurrentCount(1);
        em.persist(joeAccount);

        bobUser = new User();
        bobUser.setEmail(bobEmail);
        bobUser.setFirstName("undefined");
        bobUser.setLastName("undefined");
        bobUser.setPassword(bobPasswd);
        bobUser.setRole(Role.USER);
        em.persist(bobUser);

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
        client.addFilter(new HTTPBasicAuthFilter(adminEmail, adminPasswd));

        URI uri = UriBuilder.fromUri(String.format("http://localhost/users")).port(port).build();
        adminUsersWebResource = client.resource(uri);

        client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        uri = UriBuilder.fromUri(String.format("http://localhost/users")).port(port).build();
        joeUsersWebResource = client.resource(uri);

        client = Client.create(clientConfig);
        client.addFilter(new DefoldAuthFilter(joeEmail, null, joePasswd));
        uri = UriBuilder.fromUri(String.format("http://localhost")).port(port).build();
        joeDefoldAuthUsersWebResource = client.resource(uri);

        client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(bobEmail, bobPasswd));
        uri = UriBuilder.fromUri(String.format("http://localhost/users")).port(port).build();
        bobUsersWebResource = client.resource(uri);

        uri = UriBuilder.fromUri(String.format("http://localhost")).port(port).build();
        anonymousResource = client.resource(uri);
    }

    @Test
    public void testGetUserInfoAsAdmin() throws Exception {
        UserInfo userInfo = adminUsersWebResource
            .path(joeEmail)
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .get(UserInfo.class);

        assertEquals(joeEmail, userInfo.getEmail());
    }

    @Test
    public void testGetMyUserInfo() throws Exception {
        UserInfo userInfo = joeUsersWebResource
            .path(joeEmail)
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .get(UserInfo.class);

        assertEquals(joeEmail, userInfo.getEmail());
    }

    @Test
    public void testGetUserInfoAsJoe() throws Exception {
        UserInfo userInfo = joeUsersWebResource
            .path(bobEmail)
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .get(UserInfo.class);

        assertEquals(bobEmail, userInfo.getEmail());
    }

    @Test
    public void testGetUserInfoNotRegistred() throws Exception {
        ClientResponse response = adminUsersWebResource
            .path("notregistred@foo.com")
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .get(ClientResponse.class);

        assertEquals(404, response.getStatus());
    }

    @Test
    public void testConnections() throws Exception {

        UserInfoList joesConnections = joeUsersWebResource
            .path(String.format("/%d/connections", joeUser.getId()))
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .get(UserInfoList.class);
        assertEquals(0, joesConnections.getUsersCount());

        // First connect joe and bob
        joeUsersWebResource.path(String.format("/%d/connections/%d", joeUser.getId(), bobUser.getId())).put();

        joesConnections = joeUsersWebResource
            .path(String.format("/%d/connections", joeUser.getId()))
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .get(UserInfoList.class);
        assertEquals(1, joesConnections.getUsersCount());

        joeUsersWebResource.path(String.format("/%d/connections/%d", joeUser.getId(), bobUser.getId())).put();

        joesConnections = joeUsersWebResource
            .path(String.format("/%d/connections", joeUser.getId()))
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .get(UserInfoList.class);
        assertEquals(1, joesConnections.getUsersCount());

        UserInfoList bobsConnections = bobUsersWebResource
            .path(String.format("/%d/connections", bobUser.getId()))
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
            .get(UserInfoList.class);
        assertEquals(0, bobsConnections.getUsersCount());

        // Joe adding joe to bob (forbidden)
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/connections/%d", bobUser.getId(), joeUser.getId())).put(ClientResponse.class);
        assertEquals(403, response.getStatus());

        // Joe adding joe to himself (forbidden)
        response = joeUsersWebResource.path(String.format("/%d/connections/%d", joeUser.getId(), joeUser.getId())).put(ClientResponse.class);
        assertEquals(403, response.getStatus());

        // List other users connections (forbidden)
        response = joeUsersWebResource.path(String.format("/%d/connections", bobUser.getId())).get(ClientResponse.class);
        assertEquals(403, response.getStatus());
    }

    @Test
    public void testRegister() throws Exception {
        ObjectMapper m = new ObjectMapper();
        ObjectNode user = m.createObjectNode();
        user.put("email", "test@foo.com");
        user.put("first_name", "mr");
        user.put("last_name", "test");
        user.put("password", "verysecret");

        UserInfo userInfo = adminUsersWebResource
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .post(UserInfo.class, user.toString());

        assertEquals("test@foo.com", userInfo.getEmail());
        assertEquals("mr", userInfo.getFirstName());
        assertEquals("test", userInfo.getLastName());
    }

    @Test
    public void testRegisterTextAccept() throws Exception {
        ObjectMapper m = new ObjectMapper();
        ObjectNode user = m.createObjectNode();
        user.put("email", "test@foo.com");
        user.put("first_name", "mr");
        user.put("last_name", "test");
        user.put("password", "verysecret");

        String userInfoText = adminUsersWebResource
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .post(String.class, user.toString());

        ObjectMapper om = new ObjectMapper();
        JsonNode node = om.readValue(new ByteArrayInputStream(userInfoText.getBytes()), JsonNode.class);

        assertEquals("test@foo.com", node.get("email").getTextValue());
        assertEquals("mr", node.get("first_name").getTextValue());
        assertEquals("test", node.get("last_name").getTextValue());
    }

    @Test
    public void testRegisterAsJoe() throws Exception {
        ObjectMapper m = new ObjectMapper();
        ObjectNode user = m.createObjectNode();
        user.put("email", "test@foo.com");
        user.put("first_name", "mr");
        user.put("last_name", "test");
        user.put("password", "verysecret");

        ClientResponse response = joeUsersWebResource
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .post(ClientResponse.class, user.toString());
        assertEquals(403, response.getStatus());
    }

    @Test
    public void testDefoldAuthenticationNotLoggedIn() throws Exception {
        ClientResponse response = joeDefoldAuthUsersWebResource
            .path("users").path(joeEmail)
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .get(ClientResponse.class);

        assertEquals(Status.FORBIDDEN, response.getClientResponseStatus());
    }

    @Test
    public void testDefoldAuthentication() throws Exception {
        LoginInfo loginInfo = joeDefoldAuthUsersWebResource
            .path("/login")
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .get(LoginInfo.class);

        assertEquals(joeEmail, loginInfo.getEmail());

        // json
        UserInfo userInfo = joeDefoldAuthUsersWebResource
            .path("users").path(joeEmail)
            .accept(MediaType.APPLICATION_JSON_TYPE)
            .type(MediaType.APPLICATION_JSON_TYPE)
            .get(UserInfo.class);
        assertEquals(joeEmail, userInfo.getEmail());

        // protobuf
        userInfo = joeDefoldAuthUsersWebResource
            .path("users").path(joeEmail)
            .accept(ProtobufProviders.APPLICATION_XPROTOBUF_TYPE)
            .type(ProtobufProviders.APPLICATION_XPROTOBUF_TYPE)
            .get(UserInfo.class);
        assertEquals(joeEmail, userInfo.getEmail());
    }

    @Test
    public void testInvite() throws Exception {
        assertThat(mailer.emails.size(), is(0));

        ClientResponse response = joeUsersWebResource
            .path(String.format("/%d/invite/newuser@foo.com", joeUser.getId()))
            .put(ClientResponse.class);

        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        // Mail processes runs in a separate thread. Wait some time..
        Thread.sleep(200);
        assertThat(mailer.emails.size(), is(1));
    }

    @Test
    public void testInviteTwice() throws Exception {
        assertThat(mailer.emails.size(), is(0));
        ClientResponse response = joeUsersWebResource
            .path(String.format("/%d/invite/newuser@foo.com", joeUser.getId()))
            .put(ClientResponse.class);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        // Mail processes runs in a separate thread. Wait some time..
        Thread.sleep(200);
        assertThat(mailer.emails.size(), is(1));

        response = joeUsersWebResource
                .path(String.format("/%d/invite/newuser@foo.com", joeUser.getId()))
                .put(ClientResponse.class);
        assertEquals(Response.Status.CONFLICT.getStatusCode(), response.getStatus());

        // Mail processes runs in a separate thread. Wait some time..
        Thread.sleep(200);
        assertThat(mailer.emails.size(), is(1));
    }

    @Test
    public void testInviteProspect() throws Exception {
        EntityManager em = emf.createEntityManager();

        ClientResponse response = anonymousResource
                .path("/prospects/newuser@foo.com").put(ClientResponse.class);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        response = joeUsersWebResource
            .path(String.format("/%d/invite/newuser@foo.com", joeUser.getId()))
            .put(ClientResponse.class);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        int prospectCount = em.createQuery("select p from Prospect p").getResultList().size();
        assertThat(prospectCount, is(0));
    }

    @Test
    public void testNoRemainingInvitations() throws Exception {
        assertThat(mailer.emails.size(), is(0));
        ClientResponse response = joeUsersWebResource
            .path(String.format("/%d/invite/newuser@foo.com", joeUser.getId()))
            .put(ClientResponse.class);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        response = joeUsersWebResource
                .path(String.format("/%d/invite/newuser2@foo.com", joeUser.getId()))
                .put(ClientResponse.class);
        assertEquals(Response.Status.FORBIDDEN.getStatusCode(), response.getStatus());
    }

    @SuppressWarnings("unchecked")
    List<NewUser> newUsers(EntityManager em) {
        return em.createQuery("select u from NewUser u").getResultList();
    }

    @SuppressWarnings("unchecked")
    List<Invitation> invitations(EntityManager em) {
        return em.createQuery("select i from Invitation i").getResultList();
    }

    @Test
    public void testInviteRegistration() throws Exception {
        EntityManager em = emf.createEntityManager();

        String email = "newuser@foo.com";
        assertNull(ModelUtil.findUserByEmail(em, email));

        // Create a NewUser-entry manually. Currently we
        // can't test the OpenID-login in unit-tests
        String token = "test-token";
        em.getTransaction().begin();
        NewUser newUser = new NewUser();
        newUser.setFirstName("first");
        newUser.setLastName("last");
        newUser.setEmail(email);
        newUser.setLoginToken(token);
        em.persist(newUser);
        em.getTransaction().commit();

        // Invite newuser@foo.com
        ClientResponse response = joeUsersWebResource
            .path(String.format("/%d/invite/newuser@foo.com", joeUser.getId()))
            .put(ClientResponse.class);

        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());
        // Mail processes runs in a separate thread. Wait some time..
        Thread.sleep(200);
        assertThat(mailer.emails.size(), is(1));
        assertThat(1, is(newUsers(em).size()));
        assertThat(1, is(invitations(em).size()));

        // The message contains only the key, see config-file
        String key = mailer.emails.get(0).getMessage();
        response = anonymousResource.path("/login/openid/register/" + token)
            .queryParam("key", key)
            .put(ClientResponse.class);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        User u = ModelUtil.findUserByEmail(em, email);
        assertThat("first", is(u.getFirstName()));
        assertThat("last", is(u.getLastName()));
        InvitationAccount a = server.getInvitationAccount(em, u.getId().toString());
        assertThat(2, is(a.getOriginalCount()));
        assertThat(2, is(a.getCurrentCount()));

        assertThat(0, is(newUsers(em).size()));
        assertThat(0, is(invitations(em).size()));
    }

    @Test
    public void testInviteRegistrationDeletedInviter() throws Exception {
        EntityManager em = emf.createEntityManager();

        String email = "newuser@foo.com";
        assertNull(ModelUtil.findUserByEmail(em, email));

        // Create a NewUser-entry manually. Currently we
        // can't test the OpenID-login in unit-tests
        String token = "test-token";
        em.getTransaction().begin();
        NewUser newUser = new NewUser();
        newUser.setFirstName("first");
        newUser.setLastName("last");
        newUser.setEmail(email);
        newUser.setLoginToken(token);
        em.persist(newUser);
        em.getTransaction().commit();

        // Invite newuser@foo.com
        ClientResponse response = joeUsersWebResource
            .path(String.format("/%d/invite/newuser@foo.com", joeUser.getId()))
            .put(ClientResponse.class);

        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());
        // Mail processes runs in a separate thread. Wait some time..
        Thread.sleep(200);
        assertThat(mailer.emails.size(), is(1));
        assertThat(1, is(newUsers(em).size()));
        assertThat(1, is(invitations(em).size()));

        // Delete inviter
        em.getTransaction().begin();
        // Get managed entity
        User inviter = server.getUser(em, joeUser.getId().toString());
        // Delete
        ModelUtil.removeUser(em, inviter);
        em.getTransaction().commit();

        // The message contains only the key, see config-file
        String key = mailer.emails.get(0).getMessage();
        response = anonymousResource.path("/login/openid/register/" + token)
            .queryParam("key", key)
            .put(ClientResponse.class);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        User u = ModelUtil.findUserByEmail(em, email);
        assertThat("first", is(u.getFirstName()));
        assertThat("last", is(u.getLastName()));
        InvitationAccount a = server.getInvitationAccount(em, u.getId().toString());
        assertThat(2, is(a.getOriginalCount()));
        assertThat(2, is(a.getCurrentCount()));

        assertThat(0, is(newUsers(em).size()));
        assertThat(0, is(invitations(em).size()));
    }

    @Test
    public void testRegistrationInvalidToken() throws Exception {
        EntityManager em = emf.createEntityManager();

        String email = "newuser@foo.com";
        assertNull(ModelUtil.findUserByEmail(em, email));

        // Create a NewUser-entry manually. Currently we
        // can't test the OpenID-login in unit-tests
        String token = "test-token";
        em.getTransaction().begin();
        NewUser newUser = new NewUser();
        newUser.setFirstName("first");
        newUser.setLastName("last");
        newUser.setEmail(email);
        newUser.setLoginToken(token);
        em.persist(newUser);
        em.getTransaction().commit();

        // Invite newuser@foo.com
        ClientResponse response = joeUsersWebResource
            .path(String.format("/%d/invite/newuser@foo.com", joeUser.getId()))
            .put(ClientResponse.class);

        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());
        // Mail processes runs in a separate thread. Wait some time..
        Thread.sleep(200);
        assertThat(mailer.emails.size(), is(1));
        assertThat(1, is(newUsers(em).size()));
        assertThat(1, is(invitations(em).size()));

        // The message contains only the key, see config-file
        String key = mailer.emails.get(0).getMessage();
        response = anonymousResource.path("/login/openid/register/" + "invalid-token")
            .queryParam("key", key)
            .put(ClientResponse.class);
        assertEquals(Response.Status.BAD_REQUEST.getStatusCode(), response.getStatus());

        assertNull(ModelUtil.findUserByEmail(em, email));
        assertThat(1, is(newUsers(em).size()));
        assertThat(1, is(invitations(em).size()));
    }

    @Test
    public void testInviteInvalidKey() throws Exception {
        EntityManager em = emf.createEntityManager();

        String email = "newuser@foo.com";
        assertNull(ModelUtil.findUserByEmail(em, email));

        // Create a NewUser-entry manually. Currently we
        // can't test the OpenID-login in unit-tests
        String token = "test-token";
        em.getTransaction().begin();
        NewUser newUser = new NewUser();
        newUser.setFirstName("first");
        newUser.setLastName("last");
        newUser.setEmail(email);
        newUser.setLoginToken(token);
        em.persist(newUser);
        em.getTransaction().commit();

        // Invite newuser@foo.com
        ClientResponse response = joeUsersWebResource
            .path(String.format("/%d/invite/newuser@foo.com", joeUser.getId()))
            .put(ClientResponse.class);

        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());
        // Mail processes runs in a separate thread. Wait some time..
        Thread.sleep(200);
        assertThat(mailer.emails.size(), is(1));
        assertThat(1, is(newUsers(em).size()));
        assertThat(1, is(invitations(em).size()));

        String key = "invalid-key";
        response = anonymousResource.path("/login/openid/register/" + token)
            .queryParam("key", key)
            .put(ClientResponse.class);
        assertEquals(Response.Status.UNAUTHORIZED.getStatusCode(), response.getStatus());

        assertNull(ModelUtil.findUserByEmail(em, email));
        assertThat(1, is(newUsers(em).size()));
        assertThat(1, is(invitations(em).size()));
    }

    @Test
    public void testSubscription() throws Exception {
        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        WebResource productsResource = client.resource(uri);
        ProductInfoList productInfoList = productsResource.type(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class);

        // Create subscription
        ClientResponse response = joeUsersWebResource
.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(productInfoList.getProducts(0).getId()))
                .queryParam("external_id", "2").post(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Retrieve it
        UserSubscriptionInfo subscription = joeUsersWebResource
                .path(String.format("/%d/subscription", joeUser.getId()))
                .accept(MediaType.APPLICATION_JSON_TYPE)
.type(MediaType.APPLICATION_JSON_TYPE)
                .get(UserSubscriptionInfo.class);
        assertEquals(subscription.getProduct().getId(), productInfoList.getProducts(0).getId());

        // Activate it
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(subscription.getProduct().getId()))
                .queryParam("state", "ACTIVE").put(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Migrate it
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(productInfoList.getProducts(1).getId()))
                .queryParam("state", "ACTIVE").put(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Retrieve it
        subscription = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(UserSubscriptionInfo.class);
        assertEquals(subscription.getProduct().getId(), productInfoList.getProducts(1).getId());
        assertEquals(subscription.getState(), UserSubscriptionState.ACTIVE);

        // Delete it
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(productInfoList.getProducts(1).getId()))
                .delete(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Retrieve it to make sure it's gone
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId())).get(
                ClientResponse.class);
        assertEquals(Response.Status.NOT_FOUND.getStatusCode(), response.getStatus());
    }

    @Test
    public void testCreateSecondSubscription() throws Exception {
        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        WebResource productsResource = client.resource(uri);
        ProductInfoList productInfoList = productsResource.type(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class);

        // Create subscription
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(productInfoList.getProducts(0).getId()))
                .queryParam("external_id", "2").post(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());
        // And again
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(productInfoList.getProducts(0).getId()))
                .queryParam("external_id", "2").post(ClientResponse.class);
        assertEquals(Response.Status.CONFLICT.getStatusCode(), response.getStatus());
    }

    @Test
    public void testSetMissingSubscription() throws Exception {
        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        WebResource productsResource = client.resource(uri);
        ProductInfoList productInfoList = productsResource.type(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class);

        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(productInfoList.getProducts(1).getId()))
                .queryParam("state", "ACTIVE").put(ClientResponse.class);
        assertEquals(Response.Status.NOT_FOUND.getStatusCode(), response.getStatus());
    }

    @Test
    public void testDeleteMissingSubscription() throws Exception {
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId())).delete(
                ClientResponse.class);
        assertEquals(Response.Status.NOT_FOUND.getStatusCode(), response.getStatus());
    }

    @Test
    public void testUpdateInactiveSubscription() throws Exception {
        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        WebResource productsResource = client.resource(uri);
        ProductInfoList productInfoList = productsResource.type(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class);

        // Create subscription
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(productInfoList.getProducts(0).getId()))
                .queryParam("external_id", "2").post(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Migrate it
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(productInfoList.getProducts(1).getId()))
                .queryParam("state", "ACTIVE").put(ClientResponse.class);
        assertEquals(Response.Status.CONFLICT.getStatusCode(), response.getStatus());
    }

    @Test
    public void testUpdateProviderFail() throws Exception {
        Mockito.when(
                server.getBillingProvider().migrateSubscription(Mockito.any(UserSubscription.class),
                        Mockito.any(Product.class))).thenReturn(false);

        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        WebResource productsResource = client.resource(uri);
        ProductInfoList productInfoList = productsResource.type(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class);

        ProductInfo product = productInfoList.getProducts(0);

        // Create subscription
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(product.getId()))
                .queryParam("external_id", "2").post(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Activate it
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(product.getId())).queryParam("state", "ACTIVE")
                .put(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Migrate it
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(productInfoList.getProducts(1).getId()))
                .queryParam("state", "ACTIVE").put(ClientResponse.class);
        assertEquals(Response.Status.INTERNAL_SERVER_ERROR.getStatusCode(), response.getStatus());
    }

    @Test
    public void testUpdateToCanceled() throws Exception {
        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        WebResource productsResource = client.resource(uri);
        ProductInfoList productInfoList = productsResource.type(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class);

        // Create subscription
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(productInfoList.getProducts(0).getId()))
                .queryParam("external_id", "2").post(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Retrieve it
        UserSubscriptionInfo subscription = joeUsersWebResource
                .path(String.format("/%d/subscription", joeUser.getId())).accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(UserSubscriptionInfo.class);
        assertEquals(subscription.getProduct().getId(), productInfoList.getProducts(0).getId());

        // Activate it
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(subscription.getProduct().getId()))
                .queryParam("state", "CANCELED")
                .put(ClientResponse.class);
        assertEquals(Response.Status.CONFLICT.getStatusCode(), response.getStatus());
    }

    @Test
    public void testDeleteProviderFail() throws Exception {
        Mockito.when(
server.getBillingProvider().cancelSubscription(Mockito.any(UserSubscription.class))).thenReturn(
                false);

        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        WebResource productsResource = client.resource(uri);
        ProductInfoList productInfoList = productsResource.type(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class);

        // Create subscription
        ClientResponse response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(productInfoList.getProducts(0).getId()))
                .queryParam("external_id", "2").post(ClientResponse.class);
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Delete it
        response = joeUsersWebResource.path(String.format("/%d/subscription", joeUser.getId()))
                .queryParam("product", Long.toString(productInfoList.getProducts(1).getId()))
                .delete(ClientResponse.class);
        assertEquals(Response.Status.INTERNAL_SERVER_ERROR.getStatusCode(), response.getStatus());
    }

}

