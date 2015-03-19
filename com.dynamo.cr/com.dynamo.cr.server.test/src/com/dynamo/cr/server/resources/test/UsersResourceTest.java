package com.dynamo.cr.server.resources.test;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.when;

import java.io.ByteArrayInputStream;
import java.net.URI;
import java.util.List;

import javax.persistence.EntityManager;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.MultivaluedMap;
import javax.ws.rs.core.NewCookie;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.UriBuilder;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ObjectNode;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import com.dynamo.cr.common.providers.JsonProviders;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.proto.Config.BillingProduct;
import com.dynamo.cr.protocol.proto.Protocol.CreditCardInfo;
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
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.model.UserSubscription;
import com.dynamo.cr.server.model.UserSubscription.CreditCard;
import com.dynamo.cr.server.model.UserSubscription.State;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientHandlerException;
import com.sun.jersey.api.client.ClientRequest;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.ClientResponse.Status;
import com.sun.jersey.api.client.UniformInterfaceException;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.ClientFilter;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;

class DefoldAuthFilter extends ClientFilter {

    private String authToken;
    private String email;
    private String password;

    public DefoldAuthFilter(String email, String authToken, String password) {
        this.email = email;
        this.authToken = authToken;
        this.password = password;
    }

    @Override
    public ClientResponse handle(ClientRequest cr) throws ClientHandlerException {
        MultivaluedMap<String, Object> headers = cr.getHeaders();
        if (this.authToken != null) {
            headers.add("X-Auth", this.authToken);
        }

        headers.add("X-Email", this.email);

        // Send only password if password is set and authToken is not set.
        if (this.password != null && authToken == null) {
            headers.add("X-Password", this.password);
        }

        ClientResponse response = getNext().handle(cr);
        List<NewCookie> cookies = response.getCookies();
        for (NewCookie newCookie : cookies) {
            if (newCookie.getName().equals("auth")) {
                // Snoop auth-cookie
                this.authToken = newCookie.getValue();
            }
        }
        return response;
    }

    public void setEmail(String email) {
        this.email = email;
    }

    public void setAuthToken(String authToken) {
        this.authToken = authToken;
    }

}

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
        joeAccount.setCurrentCount(2);
        em.persist(joeAccount);

        bobUser = new User();
        bobUser.setEmail(bobEmail);
        bobUser.setFirstName("undefined");
        bobUser.setLastName("undefined");
        bobUser.setPassword(bobPasswd);
        bobUser.setRole(Role.USER);
        em.persist(bobUser);

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

        // List joes connections
        // NOTE: UserId set to -1 (unused)
        response = joeUsersWebResource.path(String.format("/%d/connections", -1)).get(ClientResponse.class);
        assertEquals(200, response.getStatus());
        UserInfoList userList = response.getEntity(UserInfoList.class);
        assertEquals(1, userList.getUsersCount());
        assertEquals(bobEmail, userList.getUsers(0).getEmail());
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
        Thread.sleep(400);
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
        Thread.sleep(400);
        assertThat(mailer.emails.size(), is(1));

        response = joeUsersWebResource
                .path(String.format("/%d/invite/newuser@foo.com", joeUser.getId()))
                .put(ClientResponse.class);
        assertEquals(Response.Status.CONFLICT.getStatusCode(), response.getStatus());

        // Mail processes runs in a separate thread. Wait some time..
        Thread.sleep(400);
        assertThat(mailer.emails.size(), is(1));
    }

    @Test
    public void testInviteProspect() throws Exception {
        EntityManager em = emf.createEntityManager();

        // Force prospect behavior and not immediate invitation
        server.setOpenRegistrationMaxUsers(0);

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
                .path(String.format("/%d/invite/newuser1@foo.com", joeUser.getId()))
                .put(ClientResponse.class);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        response = joeUsersWebResource
                .path(String.format("/%d/invite/newuser2@foo.com", joeUser.getId()))
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

    private UserSubscriptionInfo postUserSubscription(Long userId, Long externalId) {
        return joeUsersWebResource
                .path(String.format("/%d/subscription", userId))
                .queryParam("external_id", externalId.toString())
                .accept(MediaType.APPLICATION_JSON_TYPE).post(UserSubscriptionInfo.class);
    }

    private UserSubscriptionInfo getUserSubscription(Long userId) {
        return joeUsersWebResource
                .path(String.format("/%d/subscription", userId))
                .accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE)
                .get(UserSubscriptionInfo.class);
    }

    private ClientResponse putUserSubscription(Long userId, Long productId, State state, CreditCard creditCard) {
        WebResource resource = joeUsersWebResource.path(String.format("/%d/subscription", userId));
        // For mocking, we pretend that the external id is identical to the user
        // id
        UserSubscription subscription = billingProvider.getSubscription(userId);
        if (productId != null) {
            resource = resource.queryParam("product", productId.toString());
            subscription.setProductId(productId);
        }
        if (state != null) {
            resource = resource.queryParam("state", state.toString());
            subscription.setState(state);
        }
        if (creditCard != null) {
            resource = resource.queryParam("external_id", userId.toString());
            subscription.setCreditCard(creditCard);
        }
        when(billingProvider.getSubscription(userId)).thenReturn(subscription);
        return resource.type(MediaType.APPLICATION_JSON_TYPE).put(ClientResponse.class);
    }

    private ClientResponse deleteUserSubscription(Long userId) {
        return joeUsersWebResource.path(String.format("/%d/subscription", userId))
                .delete(ClientResponse.class);
    }

    private ProductInfo getProduct(WebResource productsResource, String handle) {
        return productsResource.queryParam("handle", handle).accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class).getProducts(0);
    }

    private ProductInfoList getProducts(WebResource productsResource) {
        return productsResource.accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE).get(ProductInfoList.class);
    }

    @Test
    public void testSubscription() throws Exception {
        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        WebResource productsResource = client.resource(uri);

        ProductInfo freeProduct = getProduct(productsResource, "free");
        ProductInfo smallProduct = getProduct(productsResource, "small");

        UserSubscriptionInfo subscription = postUserSubscription(joeUser.getId(), 1l);
        UserSubscription s = billingProvider.getSubscription(1l);
        assertThat(subscription, notNullValue());
        assertThat(subscription.getState().getNumber(), is(s.getState().ordinal()));
        assertThat(subscription.getProduct().getId(), is(s.getProductId()));
        assertThat(subscription.getCreditCard().getMaskedNumber(), is(s.getCreditCard()
                .getMaskedNumber()));
        assertThat(subscription.getCreditCard().getExpirationMonth(), is(s.getCreditCard()
                .getExpirationMonth()));
        assertThat(subscription.getCreditCard().getExpirationYear(), is(s.getCreditCard()
                .getExpirationYear()));

        // Retrieve it
        subscription = getUserSubscription(joeUser.getId());
        assertEquals(subscription.getProduct().getId(), freeProduct.getId());
        CreditCardInfo cc = subscription.getCreditCard();
        assertThat(cc.getMaskedNumber(), is("1"));
        assertThat(cc.getExpirationMonth(), is(1));
        assertThat(cc.getExpirationYear(), is(2020));

        // Activate it
        ClientResponse response = putUserSubscription(joeUser.getId(), null, State.ACTIVE, null);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        // Migrate it
        response = putUserSubscription(joeUser.getId(), smallProduct.getId(), null, null);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        // Update credit card
        response = putUserSubscription(joeUser.getId(), null, null, new CreditCard("2", 2, 3));
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        // Retrieve it
        subscription = getUserSubscription(joeUser.getId());
        assertEquals(smallProduct.getId(), subscription.getProduct().getId());
        assertEquals(UserSubscriptionState.ACTIVE, subscription.getState());
        cc = subscription.getCreditCard();
        assertThat(cc.getMaskedNumber(), is("2"));
        assertThat(cc.getExpirationMonth(), is(2));
        assertThat(cc.getExpirationYear(), is(3));

        // Delete it, but fail (only allowed to delete canceled subscriptions)
        response = deleteUserSubscription(joeUser.getId());
        assertEquals(Response.Status.CONFLICT.getStatusCode(), response.getStatus());

        // Set it to canceled straight in DB
        EntityManager em = emf.createEntityManager();
        s = ModelUtil.findUserSubscriptionByUser(em, joeUser);
        s.setState(State.CANCELED);
        em.getTransaction().begin();
        em.persist(s);
        em.getTransaction().commit();

        // Delete it
        response = deleteUserSubscription(joeUser.getId());
        assertEquals(Response.Status.NO_CONTENT.getStatusCode(), response.getStatus());

        // Retrieve default subscription
        subscription = getUserSubscription(joeUser.getId());
        assertEquals(freeProduct.getId(), subscription.getProduct().getId());
    }

    @Test(expected = UniformInterfaceException.class)
    public void testCreateSecondSubscription() throws Exception {

        UserSubscriptionInfo subscription = postUserSubscription(joeUser.getId(), 1l);
        assertThat(subscription, notNullValue());

        // And again
        subscription = postUserSubscription(joeUser.getId(), 1l);
    }

    @Test
    public void testSetMissingSubscription() throws Exception {
        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        WebResource productsResource = client.resource(uri);
        ProductInfo productInfo = getProduct(productsResource, "free");

        ClientResponse response = putUserSubscription(joeUser.getId(), productInfo.getId(), State.ACTIVE, null);
        assertEquals(Response.Status.NOT_FOUND.getStatusCode(), response.getStatus());
    }

    @Test
    public void testDeleteMissingSubscription() throws Exception {
        ClientResponse response = deleteUserSubscription(joeUser.getId());
        assertEquals(Response.Status.NOT_FOUND.getStatusCode(), response.getStatus());
    }

    @Test
    public void testUpdatePendingSubscription() throws Exception {

        UserSubscription subscription = billingProvider.getSubscription(0l);
        subscription.setState(State.PENDING);
        Mockito.when(billingProvider.getSubscription(Mockito.anyLong())).thenReturn(subscription);

        postUserSubscription(joeUser.getId(), 1l);

        // Migrate it
        ClientResponse response = putUserSubscription(joeUser.getId(), (long) smallProduct.getId(), null, null);
        assertEquals(Response.Status.CONFLICT.getStatusCode(), response.getStatus());

        Mockito.doAnswer(new Answer<UserSubscription>() {
            @Override
            public UserSubscription answer(InvocationOnMock invocation) throws Throwable {
                UserSubscription subscription = new UserSubscription();
                subscription.setCreditCard(new CreditCard("1", 1, 2020));
                subscription.setExternalId((Long) invocation.getArguments()[0]);
                subscription.setExternalCustomerId(1l);
                subscription.setProductId((long) freeProduct.getId());
                subscription.setState(State.ACTIVE);
                return subscription;
            }
        }).when(billingProvider).getSubscription(Mockito.anyLong());
    }

    @Test
    public void testUpdateProviderFail() throws Exception {
        Mockito.doThrow(new WebApplicationException()).when(
                server.getBillingProvider()).migrateSubscription(Mockito.any(UserSubscription.class),
                Mockito.any(BillingProduct.class));

        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        WebResource productsResource = client.resource(uri);
        ProductInfoList productInfoList = getProducts(productsResource);

        postUserSubscription(joeUser.getId(), 1l);

        // Activate it
        ClientResponse response = putUserSubscription(joeUser.getId(), null, State.ACTIVE, null);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        // Migrate it
        response = putUserSubscription(joeUser.getId(), productInfoList.getProducts(1).getId(), null, null);
        assertEquals(Response.Status.INTERNAL_SERVER_ERROR.getStatusCode(), response.getStatus());
    }

    @Test
    public void testUpdateToCanceled() throws Exception {
        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        URI uri = UriBuilder.fromUri(String.format("http://localhost/products")).port(port).build();
        WebResource productsResource = client.resource(uri);
        ProductInfoList productInfoList = getProducts(productsResource);

        postUserSubscription(joeUser.getId(), 1l);

        // Retrieve it
        UserSubscriptionInfo subscription = getUserSubscription(joeUser.getId());
        assertEquals(subscription.getProduct().getId(), productInfoList.getProducts(0).getId());

        // Terminate it
        ClientResponse response = putUserSubscription(joeUser.getId(), null, State.CANCELED, null);
        assertEquals(Response.Status.CONFLICT.getStatusCode(), response.getStatus());
    }

}

