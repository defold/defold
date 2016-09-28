package com.dynamo.cr.server.model;

import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.services.UserService;
import com.dynamo.cr.server.test.EntityManagerRule;
import com.dynamo.cr.server.test.TestUser;
import com.dynamo.cr.server.test.TestUtils;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import javax.persistence.EntityManager;
import javax.persistence.RollbackException;
import javax.persistence.TypedQuery;
import java.util.ArrayList;
import java.util.List;

import static org.junit.Assert.*;

public class ModelTest {

    @Rule
    public EntityManagerRule entityManagerRule = new EntityManagerRule();

    private EntityManager em;

    private UserService userService;

    private Project p1;
    private Project p2;

    @Before
    public void setUp() throws Exception {
        em = entityManagerRule.getEntityManager();
        userService = new UserService(em);
        createData();
    }

    @After
    public void tearDown() {
        // Some database validation
        ModelUtil.validateDatabase(em);
    }

    private void createData() {
        User u1 = TestUtils.buildUser(TestUser.CARL);
        em.persist(u1);
        User u2 = TestUtils.buildUser(TestUser.JOE);
        em.persist(u2);
        User u3 = TestUtils.buildUser(TestUser.LISA);
        em.persist(u3);

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
    }

    @Test
    public void testConnections() throws Exception {
        User u1 = ModelUtil.findUserByEmail(em, TestUser.CARL.email);
        User u2 = ModelUtil.findUserByEmail(em, TestUser.JOE.email);
        User u3 = ModelUtil.findUserByEmail(em, TestUser.LISA.email);

        assertTrue(u1.getConnections().contains(u2));
        assertTrue(u2.getConnections().contains(u1));

        assertTrue(u1.getConnections().contains(u3));
        assertTrue(u3.getConnections().contains(u1));
    }

    @Test
    public void getUser() {
        User u1 = ModelUtil.findUserByEmail(em, TestUser.CARL.email);
        assertNotNull(u1);
        assertEquals(TestUser.CARL.email, u1.getEmail());

        User u2 = ModelUtil.findUserByEmail(em, TestUser.JOE.email);
        assertNotNull(u2);
        assertEquals(TestUser.JOE.email, u2.getEmail());
    }

    @Test
    public void authenticate() {
        User u1 = ModelUtil.findUserByEmail(em, TestUser.CARL.email);
        assertTrue(u1.authenticate("carl"));
        assertFalse(u1.authenticate("xyz"));
    }

    @Test(expected=RollbackException.class)
    public void createExistingUser() {
        User u1 = new User();
        u1.setEmail(TestUser.CARL.email);
        u1.setFirstName("Carl");
        u1.setLastName("Content");
        u1.setPassword("carl");
        em.persist(u1);
        em.getTransaction().commit();
    }

    @Test(expected=RollbackException.class)
    public void createInvalidUser() {
        // null is not a valid first name
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
        User u = ModelUtil.findUserByEmail(em, TestUser.CARL.email);
        List<Project> allProjects = new ArrayList<>(u.getProjects());

        TypedQuery<Project> q = em.createQuery("select t from Project t where t.owner = :user", Project.class);
        q.setParameter("user", u);
        List<Project> myProjects = q.getResultList();

        assertEquals(2, allProjects.size());
        assertEquals(u, myProjects.get(0).getOwner());
    }

    @Test
    public void testDeleteProject() throws Exception {

        List<Project> allProjects = em.createQuery("select t from Project t", Project.class).getResultList();
        assertEquals(2, allProjects.size());

        User carl = ModelUtil.findUserByEmail(em, TestUser.CARL.email);

        TypedQuery<Project> q = em.createQuery("select t from Project t where t.owner = :user", Project.class);
        q.setParameter("user", carl);
        List<Project> carlsProjects = q.getResultList();

        Project p = carlsProjects.get(0);

        User lisa = ModelUtil.findUserByEmail(em, TestUser.LISA.email);
        ModelUtil.removeMember(p, lisa);

        ModelUtil.removeProject(em, p);
        em.getTransaction().commit();

        carl = ModelUtil.findUserByEmail(em, TestUser.CARL.email);
        carlsProjects = new ArrayList<>(carl.getProjects());
        assertEquals(1, carlsProjects.size());

        User joe = ModelUtil.findUserByEmail(em, TestUser.JOE.email);
        ArrayList<Project> joesProjects = new ArrayList<Project>(joe.getProjects());
        assertEquals(1, joesProjects.size());

        q = em.createQuery("select t from Project t where t.owner = :user", Project.class);
        q.setParameter("user", carl);
        carlsProjects = q.getResultList();
        assertEquals(0, carlsProjects.size());

        allProjects = em.createQuery("select t from Project t", Project.class).getResultList();
        assertEquals(1, allProjects.size());
    }
}

