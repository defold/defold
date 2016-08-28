package com.dynamo.cr.server.resources.test;

import com.dynamo.cr.protocol.proto.Protocol.LoginInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfoList;
import com.dynamo.cr.server.auth.Base64;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.NewUser;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.providers.JsonProviders;
import com.dynamo.cr.server.providers.ProtobufProviders;
import com.google.api.client.auth.oauth2.AuthorizationCodeRequestUrl;
import com.sun.jersey.api.client.*;
import com.sun.jersey.api.client.ClientResponse.Status;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.ClientFilter;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;
import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;
import org.junit.Before;
import org.junit.Test;

import javax.persistence.EntityManager;
import javax.ws.rs.core.*;
import java.net.URI;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.*;

class DefoldAuthFilter extends ClientFilter {

    private String authToken;
    private String email;
    private String password;

    DefoldAuthFilter(String email, String authToken, String password) {
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
    Project bobProject;
    WebResource adminUsersWebResource;
    WebResource joeUsersWebResource;
    WebResource bobUsersWebResource;
    WebResource joeDefoldAuthUsersWebResource;
    DefaultClientConfig clientConfig;
    WebResource anonymousResource;
    EntityManager em;

    @Before
    public void setUp() throws Exception {
        setupUpTest();

        em = emf.createEntityManager();
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

        bobUser = new User();
        bobUser.setEmail(bobEmail);
        bobUser.setFirstName("undefined");
        bobUser.setLastName("undefined");
        bobUser.setPassword(bobPasswd);
        bobUser.setRole(Role.USER);
        em.persist(bobUser);

        bobProject = ModelUtil.newProject(em, bobUser, "bobs_project", "bobs_project description");

        em.getTransaction().commit();

        clientConfig = new DefaultClientConfig();
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyWriter.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(adminEmail, adminPasswd));

        URI uri = UriBuilder.fromUri("http://localhost/users").port(port).build();
        adminUsersWebResource = client.resource(uri);

        client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(joeEmail, joePasswd));
        uri = UriBuilder.fromUri("http://localhost/users").port(port).build();
        joeUsersWebResource = client.resource(uri);

        client = Client.create(clientConfig);
        client.addFilter(new DefoldAuthFilter(joeEmail, null, joePasswd));
        uri = UriBuilder.fromUri("http://localhost").port(port).build();
        joeDefoldAuthUsersWebResource = client.resource(uri);

        client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(bobEmail, bobPasswd));
        uri = UriBuilder.fromUri("http://localhost/users").port(port).build();
        bobUsersWebResource = client.resource(uri);

        uri = UriBuilder.fromUri("http://localhost").port(port).build();
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

    @SuppressWarnings("unchecked")
    private List<NewUser> newUsers(EntityManager em) {
        return em.createQuery("select u from NewUser u").getResultList();
    }

    @Test
    public void testOAuthRegistration() throws Exception {
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

        assertThat(1, is(newUsers(em).size()));

        ClientResponse response = anonymousResource.path("/login/oauth/register/" + token)
            .put(ClientResponse.class);
        assertEquals(Response.Status.OK.getStatusCode(), response.getStatus());

        User u = ModelUtil.findUserByEmail(em, email);
        assertThat("first", is(u.getFirstName()));
        assertThat("last", is(u.getLastName()));

        assertThat(0, is(newUsers(em).size()));
    }

    private String getRedirectUrl(ClientResponse response) throws Exception {
        String content = response.getEntity(String.class);

        Pattern p = Pattern.compile("window.location = '([^']+)';");
        Matcher m = p.matcher(content);
        if (!m.find()) {
            return null;
        }

        return m.group(1);
    }

    @Test
    public void testOath() throws Exception {
        String redirectUrl = "/redirect";

        ClientResponse serverResponse = anonymousResource.path("/login/oauth/-1")
                .queryParam("redirect_to", redirectUrl)
                .get(ClientResponse.class);

        String locationUrl = getRedirectUrl(serverResponse);
        assertNotNull(locationUrl);
        AuthorizationCodeRequestUrl codeRequestUrl = new AuthorizationCodeRequestUrl(locationUrl, "testuser");
        String authState = Base64.base64Decode(codeRequestUrl.getState());
        ObjectMapper mapper = new ObjectMapper();
        JsonNode node = mapper.readTree(authState);

        assertEquals(200, serverResponse.getClientResponseStatus().getStatusCode());
        assertNotNull(node.get("login_token").getTextValue());
        assertEquals(redirectUrl, node.get("redirect_to").getTextValue());
    }

    @Test
    public void testOathDefaultRedirect() throws Exception {

        ClientResponse serverResponse = anonymousResource.path("/login/oauth/-1")
                .get(ClientResponse.class);

        String locationUrl = getRedirectUrl(serverResponse);
        assertNotNull(locationUrl);
        AuthorizationCodeRequestUrl codeRequestUrl = new AuthorizationCodeRequestUrl(locationUrl, "testuser");
        String authState = Base64.base64Decode(codeRequestUrl.getState());
        ObjectMapper mapper = new ObjectMapper();
        JsonNode node = mapper.readTree(authState);

        assertEquals(200, serverResponse.getClientResponseStatus().getStatusCode());
        assertNotNull(node.get("login_token").getTextValue());
        assertEquals("/", node.get("redirect_to").getTextValue());
    }

    @Test
    public void testOathAccessDenied() throws Exception {
        String redirectUrl = "/redirect";

        ClientResponse oathResponse = anonymousResource.path("/login/oauth/-1")
                .queryParam("redirect_to", redirectUrl)
                .get(ClientResponse.class);

        String locationUrl = getRedirectUrl(oathResponse);
        assertNotNull(locationUrl);
        AuthorizationCodeRequestUrl codeRequestUrl = new AuthorizationCodeRequestUrl(locationUrl, "testuser");

        ClientResponse serverResponse = anonymousResource.path("/login/oauth/-1/response")
                .queryParam("error", "access_denied")
//                .queryParam("code", "SplxlOBeZQQYbYS6WxSbIa")  // Invalid grant
                .queryParam("state", codeRequestUrl.getState())
                .get(ClientResponse.class);

        assertEquals(404, serverResponse.getStatus());
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

        assertThat(1, is(newUsers(em).size()));

        ClientResponse response = anonymousResource.path("/login/oauth/register/" + "invalid-token")
            .put(ClientResponse.class);
        assertEquals(Response.Status.BAD_REQUEST.getStatusCode(), response.getStatus());

        assertNull(ModelUtil.findUserByEmail(em, email));
        assertThat(1, is(newUsers(em).size()));
    }

    @Test
    public void testServerOkStatus() throws Exception {
        WebResource statusResource = createAnonymousResource("http://localhost/status");

        String statusResponse = statusResource.get(String.class);

        assertEquals("OK", statusResponse);
    }

    @Test
    public void testRemoveUser() throws Exception {
        ClientResponse deleteResponse = joeUsersWebResource
                .path(String.format("/%d/remove", joeUser.getId()))
                .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                .get(ClientResponse.class);

        assertEquals(200, deleteResponse.getStatus());

        try {
            // Should thrown an exception
            UserInfo userInfo = joeUsersWebResource
                    .path(joeEmail)
                    .accept(MediaType.APPLICATION_JSON_TYPE)
                    .type(MediaType.APPLICATION_JSON_TYPE)
                    .get(UserInfo.class);

            assertNull(userInfo);
        } catch(UniformInterfaceException e) {
            // User not found exception is expected
        }
    }

    @Test
    public void removeUserThatIsOwnerOfProjectWithoutMembers() throws Exception {
        // Add connection to verify that they can be deleted.
        em.getTransaction().begin();
        joeUser.getConnections().add(bobUser);
        em.getTransaction().commit();

        // Remove user.
        ClientResponse deleteResponse = bobUsersWebResource
                .path(String.format("/%d/remove", bobUser.getId()))
                .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                .get(ClientResponse.class);

        assertEquals(200, deleteResponse.getStatus());

        // Verify that user isn't available anymore.
        try {
            UserInfo userInfo = bobUsersWebResource
                    .path(bobEmail)
                    .accept(MediaType.APPLICATION_JSON_TYPE)
                    .type(MediaType.APPLICATION_JSON_TYPE)
                    .get(UserInfo.class);

            assertNull(userInfo);
        } catch(UniformInterfaceException e) {
            assertEquals(Status.UNAUTHORIZED, e.getResponse().getClientResponseStatus());
        }
    }

    @Test
    public void forbiddenToRemoveUserIfOwnerOfProjectWithOtherMembers() throws Exception {
        em.getTransaction().begin();
        bobProject.getMembers().add(joeUser);
        em.getTransaction().commit();

        ClientResponse deleteResponse = bobUsersWebResource
                .path(String.format("/%d/remove", bobUser.getId()))
                .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                .get(ClientResponse.class);

        assertEquals(Status.FORBIDDEN, deleteResponse.getClientResponseStatus());
    }

    @Test
    public void forbiddenToRemoveOtherUsers() throws Exception {
        ClientResponse deleteResponse = joeUsersWebResource
                .path(String.format("/%d/remove", bobUser.getId()))
                .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                .get(ClientResponse.class);

        assertEquals(Status.FORBIDDEN, deleteResponse.getClientResponseStatus());
    }
}

