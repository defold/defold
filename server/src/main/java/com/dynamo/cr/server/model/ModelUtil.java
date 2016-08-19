package com.dynamo.cr.server.model;

import javax.persistence.EntityManager;
import javax.persistence.TypedQuery;
import java.util.List;

public class ModelUtil {

    /**
     * Create a new project. Owner is also added to the user list (of the project)
     * @param entityManager entity manager
     * @param owner owner of the project
     * @param name name of the project
     * @param description project description
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
        entityManager.remove(project);
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

    /**
     * Subscribe to news-letter. If user is found in subscription status is kept as is
     * but first and last-name are updated. For user invited via email first and last name are unknown
     * and by calling this method after registration we can update the record.
     * This method is safe to call even if the user has unsubscribed.
     * @param em {@link EntityManager}
     * @param email email
     * @param firstName first name
     * @param lastName last name
     */
    public static void subscribeToNewsLetter(EntityManager em, String email, String firstName, String lastName) {
        TypedQuery<NewsSubscriber> q = em.createQuery("select s from NewsSubscriber s where s.email = :email", NewsSubscriber.class);
        List<NewsSubscriber> lst = q.setParameter("email", email).getResultList();
        if (lst.size() == 0) {
            NewsSubscriber ns = new NewsSubscriber();
            ns.setEmail(email);
            ns.setFirstName(firstName);
            ns.setLastName(lastName);
            em.persist(ns);
        } else {
            // If found we keep subscription status as is
            // but update first and last name
            NewsSubscriber ns = lst.get(0);
            ns.setFirstName(firstName);
            ns.setLastName(lastName);
            em.persist(ns);
        }
    }
}
