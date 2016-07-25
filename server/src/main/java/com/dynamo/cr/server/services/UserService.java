package com.dynamo.cr.server.services;

import com.dynamo.cr.server.auth.AccessTokenStore;
import com.dynamo.cr.server.model.AccessToken;
import com.dynamo.cr.server.model.InvitationAccount;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import java.util.List;
import java.util.Optional;

public class UserService {
    @Inject
    private EntityManager entityManager;

    public UserService() {
    }

    public UserService(EntityManager entityManager) {
        this.entityManager = entityManager;
    }

    public List<User> findAll() {
        return entityManager.createNamedQuery("User.findAll", User.class).getResultList();
    }

    public Optional<User> find(long userId) {
        return Optional.ofNullable(entityManager.find(User.class, userId));
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

        // Remove invitation account if exists
        InvitationAccount account = entityManager.find(InvitationAccount.class, user.getId());
        if (account != null) {
            entityManager.remove(account);
        }

        // Remove access tokens
        AccessTokenStore accessTokenStore = new AccessTokenStore(() -> entityManager);
        List<AccessToken> accessTokens = accessTokenStore.find(user);
        accessTokens.stream().forEach(accessTokenStore::delete);

        entityManager.remove(user);
    }
}
