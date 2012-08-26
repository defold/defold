package com.dynamo.cr.server.model;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.persistence.EntityManager;
import javax.persistence.TypedQuery;

import com.dynamo.cr.proto.Config.BillingProduct;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.model.UserSubscription.CreditCard;
import com.dynamo.cr.server.model.UserSubscription.State;

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

    public static Prospect findProspectByEmail(EntityManager entityManager, String email) {
        List<Prospect> list = entityManager.createQuery("select p from Prospect p where p.email = :email", Prospect.class).setParameter("email", email).getResultList();
        if (list.size() == 0) {
            return null;
        }
        else {
            assert list.size() == 1;
            return list.get(0);
        }
    }

    public static UserSubscription newUserSubscription(EntityManager entityManager, User user, long productId,
            long externalId, long externalCustomerId, CreditCard creditCard) {
        UserSubscription subscription = new UserSubscription();
        subscription.setUser(user);
        subscription.setProductId(productId);
        subscription.setExternalId(externalId);
        subscription.setExternalCustomerId(externalCustomerId);
        subscription.setCreditCard(creditCard);
        entityManager.persist(subscription);
        return subscription;
    }

    public static CreditCard newCreditCard(String maskedNumber, Integer expirationMonth, Integer expirationYear) {
        CreditCard cc = new CreditCard();
        cc.setMaskedNumber(maskedNumber);
        cc.setExpirationMonth(expirationMonth);
        cc.setExpirationYear(expirationYear);
        return cc;
    }

    public static UserSubscription findUserSubscriptionByExternalId(EntityManager entityManager, String externalId) {
        List<UserSubscription> list = entityManager
                .createQuery("select us from UserSubscription us where us.externalId = :externalId",
                        UserSubscription.class).setParameter("externalId", Long.parseLong(externalId)).getResultList();
        if (list.size() == 0) {
            return null;
        } else {
            assert list.size() == 1;
            return list.get(0);
        }
    }

    public static UserSubscription findUserSubscriptionByUser(EntityManager entityManager, User user) {
        List<UserSubscription> list = entityManager
                .createQuery("select us from UserSubscription us where us.user = :user",
                        UserSubscription.class).setParameter("user", user).getResultList();
        if (list.size() == 0) {
            return null;
        } else {
            assert list.size() == 1;
            return list.get(0);
        }
    }

    public static BillingProduct getUserProduct(EntityManager entityManager, User user, List<BillingProduct> products) {
        UserSubscription subscription = findUserSubscriptionByUser(entityManager, user);
        BillingProduct result = null;
        // Return product with identical id, default if not
        for (BillingProduct product : products) {
            if (subscription != null && subscription.getProductId() == product.getId()) {
                result = product;
                break;
            } else if (product.getDefault() != 0) {
                result = product;
            }
        }
        return result;
    }

    public static int getUserMaxMemberCount(EntityManager entityManager, User user,
            List<BillingProduct> products) {
        BillingProduct product = getUserProduct(entityManager, user, products);
        // Use default in case the subscription is canceled
        UserSubscription subscription = ModelUtil.findUserSubscriptionByUser(entityManager, user);
        if (subscription != null && subscription.getState().equals(State.CANCELED)) {
            for (BillingProduct p : products) {
                if (p.getDefault() != 0) {
                    product = p;
                    break;
                }
            }
        }
        return product.getMaxMemberCount();
    }

    public static boolean isMemberQualified(EntityManager em, User user, Project project, List<BillingProduct> products) {
        // Check membership
        if (!project.getMembers().contains(user)) {
            return false;
        }
        // Admins don't need a subscription to have access
        if (user.getRole() == Role.ADMIN) {
            return true;
        }
        int maxMemberCount = ModelUtil.getUserMaxMemberCount(em, user, products);
        // Unlimited member count is -1
        if (maxMemberCount < 0) {
            // Check subscription status
            return true;
        } else {
            int memberCount = project.getMemberCount();
            return memberCount <= maxMemberCount;
        }
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
