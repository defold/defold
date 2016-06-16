package com.dynamo.cr.server.services;

import com.dynamo.cr.server.model.User;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import java.util.List;

public class UserService {

    private final EntityManager entityManager;

    @Inject
    public UserService(EntityManager entityManager) {
        this.entityManager = entityManager;
    }

    public List<User> findAll() {
        return entityManager.createNamedQuery("User.findAll", User.class).getResultList();
    }
}
