package com.dynamo.cr.server.services;

import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.auth.AccessTokenStore;
import com.dynamo.cr.server.model.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import javax.persistence.Query;
import javax.ws.rs.core.Response;
import java.security.SecureRandom;
import java.util.List;
import java.util.Optional;

public class UserService {
    private static final Logger LOGGER = LoggerFactory.getLogger(UserService.class);
    @Inject
    private EntityManager entityManager;
    @Inject
    private SecureRandom secureRandom;

    public UserService() {
    }

    public UserService(EntityManager entityManager) {
        this.entityManager = entityManager;
    }

    public void save(User user) {
        entityManager.persist(user);
    }

    public Optional<User> find(long userId) {
        return Optional.ofNullable(entityManager.find(User.class, userId));
    }

    public Optional<User> findByEmail(String email) {
        List<User> resultList = entityManager
                .createNamedQuery("User.findByEmail", User.class)
                .setParameter("email", email)
                .getResultList();

        if (resultList.size() == 0) {
            return Optional.empty();
        }

        return Optional.of(resultList.get(0));
    }

    public User create(String token) {

        List<NewUser> list = entityManager
                .createNamedQuery("NewUser.findByLoginToken", NewUser.class)
                .setParameter("loginToken", token)
                .getResultList();

        if (list.size() == 0) {
            LOGGER.error("Unable to find NewUser row for {}", token);
            throw new ServerException("Unable to register.", Response.Status.BAD_REQUEST);
        }

        NewUser newUser = list.get(0);

        // Generate some random password for OpenID accounts.
        byte[] passwordBytes = new byte[32];
        secureRandom.nextBytes(passwordBytes);
        String password = org.apache.commons.codec.binary.Base64.encodeBase64String(passwordBytes);

        User user = new User();
        user.setEmail(newUser.getEmail());
        user.setFirstName(newUser.getFirstName());
        user.setLastName(newUser.getLastName());
        user.setPassword(password);
        entityManager.persist(user);
        entityManager.remove(newUser);
        entityManager.flush();

        LOGGER.info("New user registered: {}", user.getEmail());

        ModelUtil.subscribeToNewsLetter(entityManager, newUser.getEmail(), newUser.getFirstName(), newUser.getLastName());

        return user;
    }

    public void remove(User user) {
        // Remove user from projects user is member of
        for (Project p : user.getProjects()) {
            p.getMembers().remove(user);
        }

        // Find all users with connections to this user and remove their connection.
        List<User> connectedUsers = entityManager
                .createQuery("SELECT u FROM User u WHERE :user MEMBER OF u.connections", User.class)
                .setParameter("user", user)
                .getResultList();
        for (User connectedUser : connectedUsers) {
            connectedUser.getConnections().remove(user);
        }

        // Remove access tokens
        AccessTokenStore accessTokenStore = new AccessTokenStore(() -> entityManager);
        List<AccessToken> accessTokens = accessTokenStore.find(user);
        accessTokens.stream().forEach(accessTokenStore::delete);

        entityManager.remove(user);
    }

    public void signup(String email, String firstName, String lastName, String token) {
        // Delete old pending NewUser entries first.
        Query deleteQuery = entityManager.createNamedQuery("NewUser.delete").setParameter("email", email);

        if (deleteQuery.executeUpdate() > 0) {
            LOGGER.info("Removed old NewUser row for {}", email);
        }

        NewUser newUser = new NewUser();
        newUser.setFirstName(firstName);
        newUser.setLastName(lastName);
        newUser.setEmail(email);
        newUser.setLoginToken(token);
        entityManager.persist(newUser);
    }

}
