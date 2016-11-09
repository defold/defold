package com.dynamo.cr.server.auth;

import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.test.EntityManagerRule;
import com.dynamo.cr.server.test.FixedTimeSource;
import com.dynamo.cr.server.test.TestUser;
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

        User user = ModelUtil.findUserByEmail(entityManagerRule.getEntityManager(), TestUser.JAMES.email);
        String token = accessTokenAuthenticator.createSessionToken(user, "ip");

        assertTrue(accessTokenAuthenticator.authenticate(user, token, "some ip"));
        assertFalse(accessTokenAuthenticator.authenticate(user, token + "foo", "some ip"));
    }

    @Test
    public void sessionTokenShouldBeProlongedWhenAuthenticated() {
        AccessTokenAuthenticator accessTokenAuthenticator = injector.getInstance(AccessTokenAuthenticator.class);

        // Set time to now
        accessTokenAuthenticator.setTimeSource(new FixedTimeSource(Instant.now()));

        // Login
        User user = ModelUtil.findUserByEmail(entityManagerRule.getEntityManager(), TestUser.JAMES.email);
        String token = accessTokenAuthenticator.createSessionToken(user, "ip");

        // Step time forward to half the session prolong duration
        accessTokenAuthenticator.setTimeSource(new FixedTimeSource(Instant.now().plus(accessTokenAuthenticator.getSessionProlongDuration().dividedBy(2))));
        assertTrue(accessTokenAuthenticator.authenticate(user, token, "127.0.0.1"));

        // Step time forward a session prolong duration from now
        accessTokenAuthenticator.setTimeSource(new FixedTimeSource(Instant.now().plus(accessTokenAuthenticator.getSessionProlongDuration())));
        assertTrue(accessTokenAuthenticator.authenticate(user, token, "127.0.0.1"));
    }

    @Test
    public void expiredTokensShouldNotBeAuthenticated() {
        AccessTokenAuthenticator accessTokenAuthenticator = injector.getInstance(AccessTokenAuthenticator.class);

        User user = ModelUtil.findUserByEmail(entityManagerRule.getEntityManager(), TestUser.JAMES.email);
        accessTokenAuthenticator.setTimeSource(new FixedTimeSource(Instant.now()));
        String token = accessTokenAuthenticator.createSessionToken(user, "ip");

        assertTrue(accessTokenAuthenticator.authenticate(user, token, "some ip"));

        accessTokenAuthenticator.setTimeSource(new FixedTimeSource(Instant.now().plus(accessTokenAuthenticator.getSessionProlongDuration())));

        assertFalse(accessTokenAuthenticator.authenticate(user, token, "some ip"));
    }

    @Test
    public void lifetimeTokenShouldBeAuthenticated() {
        AccessTokenAuthenticator accessTokenAuthenticator = injector.getInstance(AccessTokenAuthenticator.class);

        User user = ModelUtil.findUserByEmail(entityManagerRule.getEntityManager(), TestUser.JAMES.email);
        String token = accessTokenAuthenticator.createLifetimeToken(user, "ip");

        assertTrue(accessTokenAuthenticator.authenticate(user, token, "some ip"));

        // Creating new token should revoke the previous one.
        String newToken = accessTokenAuthenticator.createLifetimeToken(user, "ip");
        assertFalse(accessTokenAuthenticator.authenticate(user, token, "some ip"));
        assertTrue(accessTokenAuthenticator.authenticate(user, newToken, "some ip"));
    }
}
