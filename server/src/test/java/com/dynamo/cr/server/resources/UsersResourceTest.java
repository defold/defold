package com.dynamo.cr.server.resources;

import com.dynamo.cr.protocol.proto.Protocol.*;
import com.dynamo.cr.server.auth.Base64;
import com.dynamo.cr.server.mail.EMail;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.NewUser;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.providers.JsonProviders;
import com.dynamo.cr.server.providers.ProtobufProviders;
import com.dynamo.cr.server.services.UserService;
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
import javax.ws.rs.core.Response;
import java.net.URI;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import static com.dynamo.cr.server.test.TestUser.JAMES;
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

        // Snoop auth-cookie
        cookies.stream()
                .filter(newCookie -> newCookie.getName().equals("auth"))
                .forEach(newCookie -> this.authToken = newCookie.getValue());

        return response;
    }
}

public class UsersResourceTest extends AbstractResourceTest {

    private static final String JOE_EMAIL = "joe@foo.com";
    private static final String JOE_PASSWORD = "secret2";

    private static final String BOB_EMAIL = "bob@foo.com";
    private static final String BOB_PASSWORD = "secret3";

    private static final String ADMIN_EMAIL = "admin@foo.com";
    private static final String ADMIN_PASSWORD = "secret";

    private User joeUser;
    private User adminUser;
    private User bobUser;

    private Project bobProject;

    private WebResource adminUsersWebResource;
    private WebResource joeUsersWebResource;
    private WebResource bobUsersWebResource;
    private WebResource joeDefoldAuthUsersWebResource;
    private WebResource anonymousResource;

    private EntityManager em;

    @Before
    public void setUp() throws Exception {
        setupUpTest();

        em = emf.createEntityManager();
        em.getTransaction().begin();
        adminUser = new User();
        adminUser.setEmail(ADMIN_EMAIL);
        adminUser.setFirstName("undefined");
        adminUser.setLastName("undefined");
        adminUser.setPassword(ADMIN_PASSWORD);
        adminUser.setRole(Role.ADMIN);
        em.persist(adminUser);

        joeUser = new User();
        joeUser.setEmail(JOE_EMAIL);
        joeUser.setFirstName("undefined");
        joeUser.setLastName("undefined");
        joeUser.setPassword(JOE_PASSWORD);
        joeUser.setRole(Role.USER);
        em.persist(joeUser);

        bobUser = new User();
        bobUser.setEmail(BOB_EMAIL);
        bobUser.setFirstName("undefined");
        bobUser.setLastName("undefined");
        bobUser.setPassword(BOB_PASSWORD);
        bobUser.setRole(Role.USER);
        em.persist(bobUser);

        bobProject = ModelUtil.newProject(em, bobUser, "bobs_project", "bobs_project description");

        em.getTransaction().commit();

        DefaultClientConfig clientConfig = new DefaultClientConfig();
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(JsonProviders.ProtobufMessageBodyWriter.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        clientConfig.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        Client client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(ADMIN_EMAIL, ADMIN_PASSWORD));

        URI uri = UriBuilder.fromUri("http://localhost/users").port(getServicePort()).build();
        adminUsersWebResource = client.resource(uri);

        client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(JOE_EMAIL, JOE_PASSWORD));
        uri = UriBuilder.fromUri("http://localhost/users").port(getServicePort()).build();
        joeUsersWebResource = client.resource(uri);

        client = Client.create(clientConfig);
        client.addFilter(new DefoldAuthFilter(JOE_EMAIL, null, JOE_PASSWORD));
        uri = UriBuilder.fromUri("http://localhost").port(getServicePort()).build();
        joeDefoldAuthUsersWebResource = client.resource(uri);

        client = Client.create(clientConfig);
        client.addFilter(new HTTPBasicAuthFilter(BOB_EMAIL, BOB_PASSWORD));
        uri = UriBuilder.fromUri("http://localhost/users").port(getServicePort()).build();
        bobUsersWebResource = client.resource(uri);

        client = Client.create(clientConfig);
        uri = UriBuilder.fromUri("http://localhost").port(getServicePort()).build();
        anonymousResource = client.resource(uri);
    }

    @Test
    public void testGetUserInfoAsAdmin() throws Exception {
        UserInfo userInfo = adminUsersWebResource
                .path(JOE_EMAIL)
                .accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE)
                .get(UserInfo.class);

        assertEquals(JOE_EMAIL, userInfo.getEmail());
    }

    @Test
    public void testGetMyUserInfo() throws Exception {
        UserInfo userInfo = joeUsersWebResource
                .path(JOE_EMAIL)
                .accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE)
                .get(UserInfo.class);

        assertEquals(JOE_EMAIL, userInfo.getEmail());
    }

    @Test
    public void testGetUserInfoAsJoe() throws Exception {
        UserInfo userInfo = joeUsersWebResource
                .path(BOB_EMAIL)
                .accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE)
                .get(UserInfo.class);

        assertEquals(BOB_EMAIL, userInfo.getEmail());
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
        assertEquals(BOB_EMAIL, userList.getUsers(0).getEmail());
    }

    @Test
    public void testDefoldAuthenticationNotLoggedIn() throws Exception {
        ClientResponse response = joeDefoldAuthUsersWebResource
                .path("users").path(JOE_EMAIL)
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

        assertEquals(JOE_EMAIL, loginInfo.getEmail());

        // json
        UserInfo userInfo = joeDefoldAuthUsersWebResource
                .path("users").path(JOE_EMAIL)
                .accept(MediaType.APPLICATION_JSON_TYPE)
                .type(MediaType.APPLICATION_JSON_TYPE)
                .get(UserInfo.class);
        assertEquals(JOE_EMAIL, userInfo.getEmail());

        // protobuf
        userInfo = joeDefoldAuthUsersWebResource
                .path("users").path(JOE_EMAIL)
                .accept(ProtobufProviders.APPLICATION_XPROTOBUF_TYPE)
                .type(ProtobufProviders.APPLICATION_XPROTOBUF_TYPE)
                .get(UserInfo.class);
        assertEquals(JOE_EMAIL, userInfo.getEmail());
    }

    @SuppressWarnings("unchecked")
    private List<NewUser> newUsers(EntityManager em) {
        return em.createQuery("select u from NewUser u").getResultList();
    }

    @Test
    public void testOAuthRegistration() throws Exception {
        EntityManager em = emf.createEntityManager();
        UserService userService = new UserService(em);

        String email = "newuser@foo.com";
        assertFalse(userService.findByEmail(email).isPresent());

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

        User u = userService.findByEmail(email).orElseThrow(RuntimeException::new);
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
        UserService userService = new UserService(em);

        String email = "newuser@foo.com";
        assertFalse(userService.findByEmail(email).isPresent());

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

        assertFalse(userService.findByEmail(email).isPresent());
        assertThat(1, is(newUsers(em).size()));
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
                    .path(JOE_EMAIL)
                    .accept(MediaType.APPLICATION_JSON_TYPE)
                    .type(MediaType.APPLICATION_JSON_TYPE)
                    .get(UserInfo.class);

            assertNull(userInfo);
        } catch (UniformInterfaceException e) {
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
                    .path(BOB_EMAIL)
                    .accept(MediaType.APPLICATION_JSON_TYPE)
                    .type(MediaType.APPLICATION_JSON_TYPE)
                    .get(UserInfo.class);

            assertNull(userInfo);
        } catch (UniformInterfaceException e) {
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

    @Test
    public void signupShouldResultInNewUserAndActivationEmail() {
        // Sign-up user.
        SignupRequest signupRequest = SignupRequest.newBuilder()
                .setEmail(JAMES.email)
                .setFirstName(JAMES.firstName)
                .setLastName(JAMES.lastName)
                .setPassword(JAMES.password)
                .build();

        anonymousResource
                .path("users/signup")
                .post(signupRequest);

        // Sleep for a short period to give the e-mail sending thread a chance to send activation mail.
        try {
            Thread.sleep(100);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        // Verify that activation mail has been sent.
        assertThat(mailer.getEmails().size(), is(1));

        // Extract activation token from e-mail
        EMail mail = mailer.getEmails().get(0);
        String message = mail.getMessage();
        Matcher matcher = Pattern.compile(".*\\?activationCode=([\\w-]+).*").matcher(message);
        assertTrue("Didn't find activation token in e-mail.", matcher.find());
        String token = matcher.group(1);

        // Activate user account.
        SignupVerificationResponse signupVerificationResponse = anonymousResource
                .path("users/signup/verify/" + token)
                .get(SignupVerificationResponse.class);

        // Verify that user is logged in on sign-up activation.
        anonymousResource
                .path("projects/" + signupVerificationResponse.getUserId())
                .header("X-Email", signupVerificationResponse.getEmail())
                .header("X-Auth", signupVerificationResponse.getAuth())
                .get(ProjectInfoList.class);

        // Ensure that the user can login.
        anonymousResource
                .path("login")
                .header("X-Email", JAMES.email)
                .header("X-Password", JAMES.password)
                .accept(MediaType.APPLICATION_JSON)
                .get(String.class);
    }

    @Test
    public void kingstersAreNotAllowedToSignupWithEmailAndPassword() {
        try {
            // Sign-up user.
            SignupRequest signupRequest = SignupRequest.newBuilder()
                    .setEmail("kingster@king.com")
                    .setFirstName("Carl")
                    .setLastName("Gustav")
                    .setPassword("super secret")
                    .build();

            anonymousResource
                    .path("users/signup")
                    .post(signupRequest);

            // Failure if sign-up didn't resulted in exception thrown
            fail();
        } catch (UniformInterfaceException e) {
            assertEquals(Status.CONFLICT, e.getResponse().getClientResponseStatus());
        }
    }

    @Test
    public void usersCanChangePasswordIfCurrentPasswordIsProvided() {
        // Change password
        PasswordChangeRequest passwordChangeRequest = PasswordChangeRequest.newBuilder()
                .setCurrentPassword(JOE_PASSWORD)
                .setNewPassword("sommar2016").build();
        joeUsersWebResource
                .path(joeUser.getId() + "/password/change")
                .put(passwordChangeRequest);

        // Login with new password should work
        anonymousResource
                .path("login")
                .header("X-Email", JOE_EMAIL)
                .header("X-Password", "sommar2016")
                .accept(MediaType.APPLICATION_JSON)
                .get(String.class);

        // Login with old password should not work
        try {
            anonymousResource
                    .path("login")
                    .header("X-Email", JOE_EMAIL)
                    .header("X-Password", JOE_PASSWORD)
                    .accept(MediaType.APPLICATION_JSON)
                    .get(String.class);
            fail();
        } catch (UniformInterfaceException e) {
            assertEquals(Status.UNAUTHORIZED, e.getResponse().getClientResponseStatus());
        }
    }

    @Test
    public void userPasswordCannotBeChangedWithoutCurrentPassword() {
        // Trying to change password without current password should fail.
        PasswordChangeRequest passwordChangeRequest = PasswordChangeRequest.newBuilder()
                .setCurrentPassword("wrong password")
                .setNewPassword("sommar2016").build();
        try {
            joeUsersWebResource
                    .path(joeUser.getId() + "/password/change")
                    .put(passwordChangeRequest);
            fail();
        } catch (UniformInterfaceException e) {
            assertEquals(Status.UNAUTHORIZED, e.getResponse().getClientResponseStatus());
        }
    }

    @Test
    public void passwordResetShouldBePossibleIfPasswordForgotten() {
        // Trigger a password reset e-mail
        anonymousResource
                .path("users/password/forgot")
                .queryParam("email", JOE_EMAIL)
                .post();

        // Sleep for a short period to give the e-mail sending thread a chance to send "forgot password" mail.
        try {
            Thread.sleep(100);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        // Verify that mail has been sent.
        assertThat(mailer.getEmails().size(), is(1));

        // Extract password reset token from e-mail
        EMail mail = mailer.getEmails().get(0);
        String message = mail.getMessage();
        Matcher matcher = Pattern.compile(".*\\?code=([\\w-]+).*").matcher(message);
        assertTrue("Didn't find password reset token in e-mail.", matcher.find());
        String token = matcher.group(1);

        // Reset password
        PasswordResetRequest passwordResetRequest = PasswordResetRequest.newBuilder()
                .setEmail(JOE_EMAIL)
                .setToken(token)
                .setNewPassword("I will never forget.")
                .build();
        anonymousResource
                .path("users/password/reset")
                .post(passwordResetRequest);

        // Login with new password should work
        anonymousResource
                .path("login")
                .header("X-Email", JOE_EMAIL)
                .header("X-Password", "I will never forget.")
                .accept(MediaType.APPLICATION_JSON)
                .get(String.class);

        // Login with old password should not work
        try {
            anonymousResource
                    .path("login")
                    .header("X-Email", JOE_EMAIL)
                    .header("X-Password", JOE_PASSWORD)
                    .accept(MediaType.APPLICATION_JSON)
                    .get(String.class);
            fail();
        } catch (UniformInterfaceException e) {
            assertEquals(Status.UNAUTHORIZED, e.getResponse().getClientResponseStatus());
        }

        // A second password reset with an already used token should not work
        try {
            anonymousResource
                    .path("users/password/reset")
                    .post(passwordResetRequest);
            fail();
        } catch (UniformInterfaceException e) {
            assertEquals(Status.BAD_REQUEST, e.getResponse().getClientResponseStatus());
        }
    }
}

