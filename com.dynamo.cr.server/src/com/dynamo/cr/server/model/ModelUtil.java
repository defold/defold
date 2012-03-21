package com.dynamo.cr.server.model;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.persistence.EntityManager;

public class ModelUtil {

    /**
     * Create a new project. Owner is also added to the user list (of the project)
     * @param entityManager entity manager
     * @param owner owner of the project
     * @param name name of the project
     * @param description
     * @return new project
     */
    public static Project newProject(EntityManager entityManager, User owner, String name, String description) {
        Project p = new Project();
        p.setName(name);
        p.setDescription(description);
        p.setOwner(owner);
        p.getMembers().add(owner);
        owner.getProjects().add(p);
        entityManager.persist(owner);

        return p;
    }

    /**
     * Remove project. First remove projects from every user that belongs to this project. Then the project is removed.
     * @param entityManager entity manager
     * @param project project to remove
     */
    public static void removeProject(EntityManager entityManager, Project project) {
        Set<User> users = project.getMembers();
        for (User user : users) {
            user.getProjects().remove(project);
            entityManager.persist(user);
        }
        entityManager.remove(project);
    }

    /**
     * Remove user. First user is removed to every project the user belongs to. Then the user is removed.
     * @param entityManager entity manager
     * @param user user to remove
     */
    public static void removeUser(EntityManager entityManager, User user) {
        // Remove user from projects user is member of
        for (Project p : user.getProjects()) {
            p.getMembers().remove(user);
        }

        // Remove connections to this user
        // We iterate and make updates. Make a copy
        HashSet<User> connections = new HashSet<User>(user.getConnections());
        for (User connectedUser : connections) {
            connectedUser.getConnections().remove(user);
            entityManager.persist(connectedUser);
        }

        // Remove invitation account if exists
        InvitationAccount account = entityManager.find(InvitationAccount.class, user.getId());
        if (account != null) {
            entityManager.remove(account);
        }

        entityManager.remove(user);
    }

    /**
     * Find user by email
     * @param entityManager entity manager
     * @param email user email
     * @return user. null if user is not found
     */
    public static User findUserByEmail(EntityManager entityManager, String email) {
        List<User> list = entityManager.createQuery("select u from User u where u.email = :email", User.class).setParameter("email", email).getResultList();
        if (list.size() == 0) {
            return null;
        }
        else {
            assert list.size() == 1;
            return list.get(0);
        }
    }

    public static void validateDatabase(EntityManager entityManager) {
        // Ensure that we don't have any orphaned projects
        List<Project> allProjects = entityManager.createQuery("select t from Project t", Project.class).getResultList();
        for (Project project : allProjects) {
            if (project.getMembers().size() == 0) {
                throw new RuntimeException(String.format("Invalid database. Project %s has zero user count", project));
            }
        }
    }

    public static void addMember(Project project, User user) {
        project.getMembers().add(user);
        user.getProjects().add(project);
    }

    public static void removeMember(Project project, User user) {
        project.getMembers().remove(user);
        user.getProjects().remove(project);
    }

    public static void connect(User u1, User u2) {
        u1.getConnections().add(u2);
        u2.getConnections().add(u1);
    }

}
