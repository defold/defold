package com.dynamo.cr.server.model.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import java.io.File;
import java.util.HashMap;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.persistence.NoResultException;
import javax.persistence.RollbackException;

import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.eclipse.persistence.jpa.PersistenceProvider;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.server.model.InvitationAccount;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.NewsSubscriber;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.test.Util;

public class ModelUtilTest {

    static final String joeEmail = "joe.coder@gmail.com";
    static final String bobEmail = "bob.coder@gmail.com";
    static final String joePasswd = "secret1";
    static final String bobPasswd = "secret2";

    User joeUser;
    User bobUser;

    Project bobProject;

    static final long FREE_PRODUCT_ID = 1l;
    static final long SMALL_PRODUCT_ID = 2l;

    private static final String PERSISTENCE_UNIT_NAME = "unit-test";
    private static EntityManagerFactory factory;
    private EntityManager em;


    @Before
    public void setUp() throws Exception {
        // "drop-and-create-tables" can't handle model changes correctly. We need to drop all tables first.
        // Eclipse-link only drops tables currently specified. When the model change the table set also change.
        File tmp_testdb = new File("tmp/testdb");
        if (tmp_testdb.exists()) {
            getClass().getClassLoader().loadClass("org.apache.derby.jdbc.EmbeddedDriver");
            Util.dropAllTables();
        }

        HashMap<String, Object> props = new HashMap<String, Object>();
        props.put(PersistenceUnitProperties.CLASSLOADER, this.getClass().getClassLoader());
        // NOTE: JPA-PersistenceUnits: unit-test in plug-in MANIFEST.MF has to be set. Otherwise the persistence unit is not found.
        System.setProperty(PersistenceUnitProperties.ECLIPSELINK_PERSISTENCE_XML, "META-INF/test_persistence.xml");
        factory = new PersistenceProvider().createEntityManagerFactory(PERSISTENCE_UNIT_NAME, props);
        // This is the non-OSGi way of creating a factory.
        //factory = Persistence.createEntityManagerFactory(PERSISTENCE_UNIT_NAME, props);

        em = factory.createEntityManager();
        createData();
    }

    @After
    public void tearDown() {
        em.close();
        // NOTE: Important to close the factory if the tables should be dropped and created in setUp above (drop-and-create-tables in persistance.xml)
        factory.close();
    }

    void createData() {
        em.getTransaction().begin();

        joeUser = newUser(joeEmail, "Joe", "Coder", joePasswd);
        joeUser.setRole(Role.USER);
        em.persist(joeUser);
        InvitationAccount joeAccount = newInvitationAccount(joeUser, 1);
        joeAccount.setCurrentCount(2);
        em.persist(joeAccount);

        bobUser = newUser(bobEmail, "Bob", "Coder", bobPasswd);
        bobUser.setRole(Role.USER);
        em.persist(bobUser);

        bobProject = newProject(bobUser, "bobs_project", "bobs_project description");
        em.persist(bobProject);

        addMember(bobProject, joeUser);
        connect(bobUser, joeUser);

        em.persist(bobUser);
        em.persist(joeUser);
        em.getTransaction().commit();
    }

    User newUser(String email, String firstName, String lastName, String password) {
        User u = new User();
        u.setEmail(email);
        u.setFirstName(firstName);
        u.setLastName(lastName);
        u.setPassword(password);
        return u;
    }

    InvitationAccount newInvitationAccount(User user, int originalCount) {
        InvitationAccount account = new InvitationAccount();
        account.setUser(user);
        account.setOriginalCount(originalCount);
        account.setCurrentCount(originalCount);
        return account;
    }

    Project newProject(User owner, String name, String description) {
        Project p = new Project();
        p.setName(name);
        p.setDescription(description);
        p.setOwner(owner);
        p.getMembers().add(owner);
        owner.getProjects().add(p);
        em.persist(owner);
        return p;
    }

    void addMember(Project project, User user) {
        project.getMembers().add(user);
        user.getProjects().add(project);
    }

    void removeMember(Project project, User user) {
        project.getMembers().remove(user);
        user.getProjects().remove(project);
    }

    void connect(User u1, User u2) {
        u1.getConnections().add(u2);
        u2.getConnections().add(u1);
    }

    NewsSubscriber getNewsSubscriber(String email) {
        try {
            return em.createQuery("select s from NewsSubscriber s where s.email = :email", NewsSubscriber.class)
                .setParameter("email", email)
                .getSingleResult();
        } catch (NoResultException nre) {
            return null;
        }
    }


    @Test
    public void testCountProjectMembers() throws Exception {
        long bobProjectCount = ModelUtil.getProjectCount(em, bobUser);
        assertEquals(1, bobProjectCount);

        long joeProjectCount = ModelUtil.getProjectCount(em, joeUser);
        assertEquals(0, joeProjectCount);

        // Remove owner from the project member list
        removeMember(bobProject, bobUser);
        bobProjectCount = ModelUtil.getProjectCount(em, bobUser);
        assertEquals(1, bobProjectCount);
    }

    @Test
    public void testSubscribeToNewsletter() throws Exception {
        String userLastName = "Coder";
        NewsSubscriber existingSubscriber = getNewsSubscriber(bobEmail);
        assertNull(existingSubscriber);

        em.getTransaction().begin();
        ModelUtil.subscribeToNewsLetter(em, bobEmail, "Bob", userLastName);
        em.getTransaction().commit();

        existingSubscriber = getNewsSubscriber(bobEmail);
        assertEquals(bobEmail, existingSubscriber.getEmail());
        assertEquals(userLastName, existingSubscriber.getLastName());
    }


    @Test
    public void testUpdateSubscriberName() throws Exception {
        String userFirstName = "Bob";
        String userLastName = "Coder";

        em.getTransaction().begin();
        // Add new subscriber
        ModelUtil.subscribeToNewsLetter(em, bobEmail, "random", "random");
        em.getTransaction().commit();

        em.getTransaction().begin();
        // Update name details
        ModelUtil.subscribeToNewsLetter(em, bobEmail, userFirstName, userLastName);
        em.getTransaction().commit();

        NewsSubscriber subscriber = getNewsSubscriber(bobEmail);
        assertEquals(userLastName, subscriber.getLastName());
        assertEquals(userFirstName, subscriber.getFirstName());
    }

    @Test
    public void testRemoveUser() throws Exception {
        Long joeUserId = joeUser.getId();
        em.getTransaction().begin();
        ModelUtil.removeUser(em, joeUser);
        em.getTransaction().commit();

        for(User member : bobProject.getMembers()) {
            Assert.assertNotEquals(member.getId(), joeUserId);
        }

        for(User connection : bobUser.getConnections()) {
            Assert.assertNotEquals(connection.getId(), joeUserId);
        }

        InvitationAccount account = em.find(InvitationAccount.class, joeUserId);
        assertNull(account);

        User joeUser = em.find(User.class, joeUserId);
        assertNull(joeUser);
    }

    @Test(expected=RollbackException.class)
    public void testRemoveUserWithProject() throws Exception {
        em.getTransaction().begin();
        ModelUtil.removeUser(em, bobUser);
        em.getTransaction().commit();
    }

}
