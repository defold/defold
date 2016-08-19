package com.dynamo.cr.server.model.test;

import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.NewsSubscriber;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.services.UserService;
import com.dynamo.cr.server.test.EntityManagerRule;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import javax.persistence.EntityManager;
import javax.persistence.NoResultException;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

public class ModelUtilTest {

    @Rule
    public EntityManagerRule entityManagerRule = new EntityManagerRule();

    private EntityManager entityManager;
    private UserService userService;

    private static final String JOE_EMAIL = "joe.coder@gmail.com";
    private static final String BOB_EMAIL = "bob.coder@gmail.com";
    private static final String JOE_PASSWD = "secret1";
    private static final String BOB_PASSWD = "secret2";

    private User joeUser;
    private User bobUser;
    private Project bobProject;

    @Before
    public void setUp() throws Exception {
        entityManager = entityManagerRule.getEntityManager();
        userService = new UserService(entityManager);
        createData();
    }

    private void createData() {
        joeUser = newUser(JOE_EMAIL, "Joe", "Coder", JOE_PASSWD);
        joeUser.setRole(Role.USER);
        entityManager.persist(joeUser);

        bobUser = newUser(BOB_EMAIL, "Bob", "Coder", BOB_PASSWD);
        bobUser.setRole(Role.USER);
        entityManager.persist(bobUser);

        bobProject = newProject(bobUser, "bobs_project", "bobs_project description");
        entityManager.persist(bobProject);

        addMember(bobProject, joeUser);
        connect(bobUser, joeUser);

        entityManager.persist(bobUser);
        entityManager.persist(joeUser);
        entityManager.getTransaction().commit();
    }

    private User newUser(String email, String firstName, String lastName, String password) {
        User u = new User();
        u.setEmail(email);
        u.setFirstName(firstName);
        u.setLastName(lastName);
        u.setPassword(password);
        return u;
    }

    private Project newProject(User owner, String name, String description) {
        Project p = new Project();
        p.setName(name);
        p.setDescription(description);
        p.setOwner(owner);
        p.getMembers().add(owner);
        owner.getProjects().add(p);
        entityManager.persist(owner);
        return p;
    }

    private void addMember(Project project, User user) {
        project.getMembers().add(user);
        user.getProjects().add(project);
    }

    private void connect(User u1, User u2) {
        u1.getConnections().add(u2);
        u2.getConnections().add(u1);
    }

    private NewsSubscriber getNewsSubscriber(String email) {
        try {
            return entityManager.createQuery("select s from NewsSubscriber s where s.email = :email", NewsSubscriber.class)
                .setParameter("email", email)
                .getSingleResult();
        } catch (NoResultException nre) {
            return null;
        }
    }

    @Test
    public void testSubscribeToNewsletter() throws Exception {
        String userLastName = "Coder";
        NewsSubscriber existingSubscriber = getNewsSubscriber(BOB_EMAIL);
        assertNull(existingSubscriber);

        entityManager.getTransaction().begin();
        ModelUtil.subscribeToNewsLetter(entityManager, BOB_EMAIL, "Bob", userLastName);
        entityManager.getTransaction().commit();

        existingSubscriber = getNewsSubscriber(BOB_EMAIL);
        assertEquals(BOB_EMAIL, existingSubscriber.getEmail());
        assertEquals(userLastName, existingSubscriber.getLastName());
    }

    @Test
    public void testUpdateSubscriberName() throws Exception {
        String userFirstName = "Bob";
        String userLastName = "Coder";

        entityManager.getTransaction().begin();
        // Add new subscriber
        ModelUtil.subscribeToNewsLetter(entityManager, BOB_EMAIL, "random", "random");
        entityManager.getTransaction().commit();

        entityManager.getTransaction().begin();
        // Update name details
        ModelUtil.subscribeToNewsLetter(entityManager, BOB_EMAIL, userFirstName, userLastName);
        entityManager.getTransaction().commit();

        NewsSubscriber subscriber = getNewsSubscriber(BOB_EMAIL);
        assertEquals(userLastName, subscriber.getLastName());
        assertEquals(userFirstName, subscriber.getFirstName());
    }

    @Test
    public void testRemoveUser() throws Exception {
        Long joeUserId = joeUser.getId();
        entityManager.getTransaction().begin();
        userService.remove(joeUser);
        entityManager.getTransaction().commit();

        for(User member : bobProject.getMembers()) {
            Assert.assertNotEquals(member.getId(), joeUserId);
        }

        for(User connection : bobUser.getConnections()) {
            Assert.assertNotEquals(connection.getId(), joeUserId);
        }

        User joeUser = entityManager.find(User.class, joeUserId);
        assertNull(joeUser);
    }
}
