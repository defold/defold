package com.dynamo.cr.server.auth;

import com.dynamo.cr.server.model.AccessToken;
import com.dynamo.cr.server.model.User;

import javax.inject.Inject;
import javax.inject.Provider;
import javax.persistence.EntityManager;
import javax.persistence.TypedQuery;
import java.util.List;

public class AccessTokenStore {
    private final Provider<EntityManager> entityManagerProvider;

    @Inject
    public AccessTokenStore(Provider<EntityManager> entityManagerProvider) {
        this.entityManagerProvider = entityManagerProvider;
    }

    public void store(AccessToken accessToken) {
        entityManagerProvider.get().persist(accessToken);
    }

    public void storeInTransaction(AccessToken accessToken) {
        EntityManager entityManager = entityManagerProvider.get();
        entityManager.getTransaction().begin();
        entityManager.persist(accessToken);
        entityManager.getTransaction().commit();
    }

    public List<AccessToken> find(User user) {
        TypedQuery<AccessToken> query = entityManagerProvider.get().createNamedQuery("AccessToken.findUserTokens", AccessToken.class);
        query.setParameter("user", user);
        return query.getResultList();
    }

    public void delete(AccessToken accessToken) {
        EntityManager entityManager = entityManagerProvider.get();

        /*
            The accessToken object might be in detached mode when provided in some cases
            (for instance when the call origins from GitSecurityFilter). Therefore it has to be merged to the current
            entity manager before removal.
         */
        AccessToken merge = entityManager.merge(accessToken);
        entityManager.remove(merge);
    }
}
