package com.dynamo.cr.server.model.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Set;
import java.util.UUID;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.persistence.RollbackException;
import javax.persistence.TypedQuery;

import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.eclipse.persistence.jpa.PersistenceProvider;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.server.model.Invitation;
import com.dynamo.cr.server.model.InvitationAccount;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.Prospect;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.test.Util;

public class ModelTest {

    static final String CARL_CONTENT_EMAIL = "carl.content@gmail.com";
    static final String JOE_CODER_EMAIL = "joe.coder@gmail.com";
    static final String LISA_USER_EMAIL = "lisa.user@gmail.com";

    static final long FREE_PRODUCT_ID = 1l;
    static final long SMALL_PRODUCT_ID = 2l;

    private static final String PERSISTENCE_UNIT_NAME = "unit-test";
    private static EntityManagerFactory factory;
    private EntityManager em;

    Project p1;
    Project p2;

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
        // Some database validation
        ModelUtil.validateDatabase(em);

        em.close();
        // NOTE: Important to close the factory if the tables should be dropped and created in setUp above (drop-and-create-tables in persistance.xml)
        factory.close();
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

    void createData() {
        em.getTransaction().begin();

        User u1 = newUser(CARL_CONTENT_EMAIL, "Carl", "Content", "carl");
        em.persist(u1);
        InvitationAccount a1 = newInvitationAccount(u1, 1);
        em.persist(a1);
        User u2 = newUser(JOE_CODER_EMAIL, "Joe", "Coder", "joe");
        em.persist(u2);
        InvitationAccount a2 = newInvitationAccount(u2, 1);
        em.persist(a2);
        User u3 = newUser(LISA_USER_EMAIL, "Lisa", "User", "lisa");
        em.persist(u3);
        InvitationAccount a3 = newInvitationAccount(u3, 1);
        em.persist(a3);

        // Create new projects
        p1 = ModelUtil.newProject(em, u1, "Carl Contents Project", "Carl Contents Project Description");
        p2 = ModelUtil.newProject(em, u2, "Joe Coders' Project", "Joe Coders' Project Description");

        // Add users to project
        ModelUtil.addMember(p1, u2);
        ModelUtil.addMember(p1, u3);
        ModelUtil.addMember(p2, u1);
        ModelUtil.addMember(p2, u3);

        // Set up connections
        ModelUtil.connect(u1, u2);
        ModelUtil.connect(u1, u3);

        em.persist(u1);
        em.persist(u2);
        em.persist(u3);

        em.getTransaction().commit();
    }

    @Test
    public void testConnections() throws Exception {
        User u1 = ModelUtil.findUserByEmail(em, CARL_CONTENT_EMAIL);
        User u2 = ModelUtil.findUserByEmail(em, JOE_CODER_EMAIL);
        User u3 = ModelUtil.findUserByEmail(em, LISA_USER_EMAIL);

        assertTrue(u1.getConnections().contains(u2));
        assertTrue(u2.getConnections().contains(u1));

        assertTrue(u1.getConnections().contains(u3));
        assertTrue(u3.getConnections().contains(u1));
    }

    @Test
    public void getUser() {
        User u1 = ModelUtil.findUserByEmail(em, CARL_CONTENT_EMAIL);
        assertNotNull(u1);
        assertEquals(CARL_CONTENT_EMAIL, u1.getEmail());

        User u2 = ModelUtil.findUserByEmail(em, JOE_CODER_EMAIL);
        assertNotNull(u2);
        assertEquals(JOE_CODER_EMAIL, u2.getEmail());
    }

    @Test
    public void authenticate() {
        User u1 = ModelUtil.findUserByEmail(em, CARL_CONTENT_EMAIL);
        assertTrue(u1.authenticate("carl"));
        assertFalse(u1.authenticate("xyz"));
    }

    @Test(expected=RollbackException.class)
    public void createExistingUser() {
        em.getTransaction().begin();

        User u1 = new User();
        u1.setEmail(CARL_CONTENT_EMAIL);
        u1.setFirstName("Carl");
        u1.setLastName("Content");
        u1.setPassword("carl");
        em.persist(u1);

        em.getTransaction().commit();
    }

    @Test(expected=RollbackException.class)
    public void createInvalidUser() {
        // null is not a valid first name
        em.getTransaction().begin();

        User u1 = new User();
        u1.setEmail("foo@bar.com");
        u1.setFirstName(null);
        u1.setLastName("Content");
        u1.setPassword("foo");
        em.persist(u1);

        em.getTransaction().commit();
    }

    @Test
    public void testProject() throws Exception {
        User u = ModelUtil.findUserByEmail(em, CARL_CONTENT_EMAIL);
        List<Project> allProjects = new ArrayList<Project>(u.getProjects());

        TypedQuery<Project> q = em.createQuery("select t from Project t where t.owner = :user", Project.class);
        q.setParameter("user", u);
        List<Project> myProjects = q.getResultList();

        assertEquals(2, allProjects.size());
        assertEquals(u, myProjects.get(0).getOwner());
    }

    @Test(expected=RollbackException.class)
    public void testRemoveProjectConstraintViolation() throws Exception {
        // Test that we can't remove a project that has owners.
        User u = ModelUtil.findUserByEmail(em, CARL_CONTENT_EMAIL);

        TypedQuery<Project> q = em.createQuery("select t from Project t where t.owner = :user", Project.class);
        q.setParameter("user", u);
        List<Project> projects = q.getResultList();

        assertEquals(1, projects.size());

        Project p = projects.get(0);

        em.getTransaction().begin();
        u.getProjects().remove(p);
        em.remove(p);
        em.getTransaction().commit();

        u = ModelUtil.findUserByEmail(em, CARL_CONTENT_EMAIL);
    }

    @Test(expected=RollbackException.class)
    public void testRemoveUserConstraintViolation() throws Exception {
        // Test that we can't remove a user that is part of projects
        User u = ModelUtil.findUserByEmail(em, LISA_USER_EMAIL);
        em.getTransaction().begin();
        em.remove(u);
        em.getTransaction().commit();
    }

    @Test(expected=RollbackException.class)
    public void testRemoveUserOwnerOfProjectsConstraintViolation() throws Exception {
        // User owns projects
        User u = ModelUtil.findUserByEmail(em, CARL_CONTENT_EMAIL);
        em.getTransaction().begin();
        ModelUtil.removeUser(em, u);
        em.getTransaction().commit();
    }

    public void testRemoveUserOwnerOfProjects() throws Exception {
        List<Project> allProjects = em.createQuery("select t from Project t", Project.class).getResultList();
        assertEquals(2, allProjects.size());

        User u = ModelUtil.findUserByEmail(em, CARL_CONTENT_EMAIL);
        Set<Project> projects = u.getProjects();
        em.getTransaction().begin();
        // First remove all projects the user owns
        for (Project p : projects) {
            if (p.getOwner().equals(u)) {
                ModelUtil.removeProject(em, p);
            }
        }
        // Then remove user
        ModelUtil.removeUser(em, u);
        em.getTransaction().commit();

        allProjects = em.createQuery("select t from Project t", Project.class).getResultList();
        assertEquals(1, allProjects.size());
    }

    @Test
    public void testRemoveUserNotOwnerOfProjects() throws Exception {
        // User doesn't owns any projects
        User u = ModelUtil.findUserByEmail(em, LISA_USER_EMAIL);
        em.getTransaction().begin();
        ModelUtil.removeUser(em, u);
        em.getTransaction().commit();
    }

    @Test
    public void testDeleteProject() throws Exception {

        List<Project> allProjects = em.createQuery("select t from Project t", Project.class).getResultList();
        assertEquals(2, allProjects.size());

        User carl = ModelUtil.findUserByEmail(em, CARL_CONTENT_EMAIL);

        TypedQuery<Project> q = em.createQuery("select t from Project t where t.owner = :user", Project.class);
        q.setParameter("user", carl);
        List<Project> carlsProjects = q.getResultList();

        Project p = carlsProjects.get(0);

        User lisa = ModelUtil.findUserByEmail(em, LISA_USER_EMAIL);
        ModelUtil.removeMember(p, lisa);

        em.getTransaction().begin();
        ModelUtil.removeProject(em, p);
        em.getTransaction().commit();

        carl = ModelUtil.findUserByEmail(em, CARL_CONTENT_EMAIL);
        carlsProjects = new ArrayList<Project>(carl.getProjects());
        assertEquals(1, carlsProjects.size());

        User joe = ModelUtil.findUserByEmail(em, JOE_CODER_EMAIL);
        ArrayList<Project> joesProjects = new ArrayList<Project>(joe.getProjects());
        assertEquals(1, joesProjects.size());

        q = em.createQuery("select t from Project t where t.owner = :user", Project.class);
        q.setParameter("user", carl);
        carlsProjects = q.getResultList();
        assertEquals(0, carlsProjects.size());

        allProjects = em.createQuery("select t from Project t", Project.class).getResultList();
        assertEquals(1, allProjects.size());
    }

    @Test
    public void testInvitationAccount() throws Exception {
        em.getTransaction().begin();
        User inviter = newUser("inviter@foo.com", "Mr", "Inviter", "123");
        InvitationAccount account = newInvitationAccount(inviter, 1);
        em.persist(inviter);
        em.persist(account);
        em.getTransaction().commit();

        em.getTransaction().begin();
        ModelUtil.removeUser(em, inviter);
        em.getTransaction().commit();
    }

    @Test
    public void testInvitation() throws Exception {
        em.getTransaction().begin();
        User inviter = newUser("inviter@foo.com", "Mr", "Inviter", "123");
        em.persist(inviter);
        InvitationAccount account = newInvitationAccount(inviter, 1);
        em.persist(account);
        em.getTransaction().commit();

        Invitation invitation = new Invitation();
        invitation.setEmail("foo@bar.com");
        invitation.setInviterEmail(inviter.getEmail());
        invitation.setInitialInvitationCount(1);
        invitation.setRegistrationKey(UUID.randomUUID().toString());
        em.getTransaction().begin();
        em.persist(invitation);
        em.getTransaction().commit();

        em.getTransaction().begin();
        ModelUtil.removeUser(em, inviter);
        em.getTransaction().commit();

        List<Invitation> lst = em.createQuery("select i from Invitation i", Invitation.class).getResultList();
        for (Invitation i : lst) {
            System.out.println(i);
        }
    }

    @Test(expected=RollbackException.class)
    public void testInvitationDuplicate() throws Exception {
        em.getTransaction().begin();
        User inviter = newUser("inviter@foo.com", "Mr", "Inviter", "123");
        em.persist(inviter);
        InvitationAccount account = newInvitationAccount(inviter, 1);
        em.persist(account);

        Invitation invitation = new Invitation();
        invitation.setEmail("foo@bar.com");
        invitation.setInviterEmail(inviter.getEmail());
        invitation.setInitialInvitationCount(1);
        invitation.setRegistrationKey(UUID.randomUUID().toString());
        em.persist(invitation);

        invitation = new Invitation();
        invitation.setEmail("foo@bar.com");
        invitation.setInviterEmail(inviter.getEmail());
        invitation.setInitialInvitationCount(1);
        invitation.setRegistrationKey(UUID.randomUUID().toString());
        em.persist(invitation);
        em.getTransaction().commit();
    }

    @Test
    public void testProspect() throws Exception {
        Prospect prospect = new Prospect();
        prospect.setEmail("foo@bar.com");
        em.getTransaction().begin();
        em.persist(prospect);
        Date d = new Date();
        em.getTransaction().commit();

        List<Prospect> lst = em.createQuery("select p from Prospect p", Prospect.class).getResultList();
        assertThat(lst.size(), is(1));
        Prospect p = lst.get(0);
        assertThat(p.getEmail(), is("foo@bar.com"));
        boolean recently = (p.getDate().getTime() - d.getTime()) < 100;
        assertTrue(recently);
    }

    @Test(expected=RollbackException.class)
    public void testProspectDuplicate() throws Exception {
        Prospect prospect = new Prospect();
        prospect.setEmail("foo@bar.com");
        em.getTransaction().begin();
        em.persist(prospect);
        em.getTransaction().commit();

        prospect = new Prospect();
        prospect.setEmail("foo@bar.com");
        em.getTransaction().begin();
        em.persist(prospect);
        em.getTransaction().commit();
    }

}

