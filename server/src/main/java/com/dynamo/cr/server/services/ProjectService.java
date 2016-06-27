package com.dynamo.cr.server.services;

import com.dynamo.cr.server.model.Project;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import java.util.Optional;

public class ProjectService {
    @Inject
    private EntityManager entityManager;

    public Optional<Project> find(Long projectId) {
        return Optional.ofNullable(entityManager.find(Project.class, projectId));
    }
}
