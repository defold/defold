package com.dynamo.cr.server.services;

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
}
