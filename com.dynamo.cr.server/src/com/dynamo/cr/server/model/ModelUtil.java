package com.dynamo.cr.server.model;

import java.util.List;
import java.util.Set;

import javax.persistence.EntityManager;

public class ModelUtil {

    public static Project newProject(EntityManager entityManager, User owner, String name) {
        Project p = new Project();
        p.setName(name);
        p.setOwner(owner);
        p.getUsers().add(owner);
        owner.getProjects().add(p);
        entityManager.persist(owner);

        return p;
    }

    public static void deleteProject(EntityManager entityManager, Project project) {
        Set<User> users = project.getUsers();
        for (User user : users) {
            user.getProjects().remove(project);
        }
        entityManager.remove(project);
    }

    public static void validateDatabase(EntityManager entityManager) {
        // Ensure that we don't have any orphaned projects
        List<Project> allProjects = entityManager.createQuery("select t from Project t", Project.class).getResultList();
        for (Project project : allProjects) {
            if (project.getUsers().size() == 0) {
                throw new RuntimeException(String.format("Invalid database. Project %s has zero user count", project));
            }
        }
    }

}
