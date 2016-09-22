package com.dynamo.cr.server;

import javax.inject.Inject;
import javax.inject.Provider;
import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;

public class EntityManagerProvider implements Provider<EntityManager> {
    private final EntityManagerFactory entityManagerFactory;

    @Inject
    public EntityManagerProvider(EntityManagerFactory entityManagerFactory) {
        this.entityManagerFactory = entityManagerFactory;
    }

    @Override
    public EntityManager get() {
        return entityManagerFactory.createEntityManager();
    }
}
