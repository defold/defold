package com.dynamo.cr.server.auth;

import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.test.EntityManagerRule;
import com.dynamo.cr.server.test.FixedTimeSource;
import com.dynamo.cr.server.test.TestUtils;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import javax.persistence.EntityManager;
import java.time.Instant;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

public class AccessTokenAuthenticatorTest {
    @Rule
    public EntityManagerRule entityManagerRule = new EntityManagerRule();

    private final Injector injector;

    private class Module extends AbstractModule {

        @Override
        protected void configure() {
            bind(EntityManager.class).toProvider(entityManagerRule.getEntityManagerProvider());
            bind(AccessTokenStore.class);
            bind(AccessTokenAuthenticator.class);
        }
    }

    public AccessTokenAuthenticatorTest() {
        Module module = new Module();
        injector = Guice.createInjector(module);
    }

    @Before
    public void before() throws Exception {
        TestUtils.createTestUsers(entityManagerRule.getEntityManager());
    }

    @Test
    public void sessionTokenShouldBeAuthenticated() {
        AccessTokenAuthenticator accessTokenAuthenticator = injector.getInstance(AccessTokenAuthenticator.class);

        User user = ModelUtil.findUserByEmail(entityManagerRule.getEntityManager(), TestUtils.TestUser.JAMES.email);
        String token = accessTokenAuthenticator.createSessionToken(user, "ip");

        assertTrue(accessTokenAuthenticator.authenticate(user, token, "some ip"));
        assertFalse(accessTokenAuthenticator.authenticate(user, token + "foo", "some ip"));
    }

    @Test
    public void expiredTokensShouldNotBeAuthenticated() {
        AccessTokenAuthenticator accessTokenAuthenticator = injector.getInstance(AccessTokenAuthenticator.class);

        User user = ModelUtil.findUserByEmail(entityManagerRule.getEntityManager(), TestUtils.TestUser.JAMES.email);
        accessTokenAuthenticator.setTimeSource(new FixedTimeSource(Instant.now()));
        String token = accessTokenAuthenticator.createSessionToken(user, "ip");

        assertTrue(accessTokenAuthenticator.authenticate(user, token, "some ip"));

        accessTokenAuthenticator.setTimeSource(new FixedTimeSource(Instant.now().plus(accessTokenAuthenticator.getSessionProlongDuration())));

        assertFalse(accessTokenAuthenticator.authenticate(user, token, "some ip"));
    }

    @Test
    public void lifetimeTokenShouldBeAuthenticated() {
        AccessTokenAuthenticator accessTokenAuthenticator = injector.getInstance(AccessTokenAuthenticator.class);

        User user = ModelUtil.findUserByEmail(entityManagerRule.getEntityManager(), TestUtils.TestUser.JAMES.email);
        String token = accessTokenAuthenticator.createLifetimeToken(user, "ip");

        assertTrue(accessTokenAuthenticator.authenticate(user, token, "some ip"));

        // Creating new token should revoke the previous one.
        String newToken = accessTokenAuthenticator.createLifetimeToken(user, "ip");
        assertFalse(accessTokenAuthenticator.authenticate(user, token, "some ip"));
        assertTrue(accessTokenAuthenticator.authenticate(user, newToken, "some ip"));
    }
}
