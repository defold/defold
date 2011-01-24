package com.dynamo.cr.server.model.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.persistence.RollbackException;
import javax.persistence.TypedQuery;

import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.eclipse.persistence.jpa.osgi.PersistenceProvider;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;

public class ModelTest {

    static final String CARL_CONTENT_EMAIL = "carl.content@gmail.com";
    static final String JOE_CODER_EMAIL = "joe.coder@gmail.com";
    private static final String PERSISTENCE_UNIT_NAME = "unit-test";
    private static EntityManagerFactory factory;
    private EntityManager em;

    @Before
    public void setUp() {
        HashMap<String, Object> props = new HashMap<String, Object>();
        props.put(PersistenceUnitProperties.CLASSLOADER, this.getClass().getClassLoader());
        // NOTE: JPA-PersistenceUnits: unit-test in plug-in MANIFEST.MF has to be set. Otherwise the persistence unit is not found.
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

    void createData() {
        em.getTransaction().begin();

        User u1 = new User();
        u1.setEmail(CARL_CONTENT_EMAIL);
        u1.setFirstName("Carl");
        u1.setLastName("Content");
        em.persist(u1);

        User u2 = new User();
        u2.setEmail(JOE_CODER_EMAIL);
        u2.setFirstName("Joe");
        u2.setLastName("Coder");
        em.persist(u2);

        // Create new projects
        Project p1 = ModelUtil.newProject(em, u1, "Carl Contents Project");
        Project p2 = ModelUtil.newProject(em, u2, "Joe Coders' Project");

        // Add users to project
        u2.getProjects().add(p1);
        u1.getProjects().add(p2);

        p1.getUsers().add(u2);
        p2.getUsers().add(u1);

        em.persist(u1);
        em.persist(u2);

        em.getTransaction().commit();
    }

    @Test
    public void getUser() {
        User u1 = em.find(User.class, CARL_CONTENT_EMAIL);
        assertNotNull(u1);
        assertEquals(CARL_CONTENT_EMAIL, u1.getEmail());

        User u2 = em.find(User.class, JOE_CODER_EMAIL);
        assertNotNull(u2);
        assertEquals(JOE_CODER_EMAIL, u2.getEmail());
    }

    @Test(expected=RollbackException.class)
    public void createExistingUser() {
        em.getTransaction().begin();

        User u1 = new User();
        u1.setEmail(CARL_CONTENT_EMAIL);
        u1.setFirstName("Carl");
        u1.setLastName("Content");
        em.persist(u1);

        em.getTransaction().commit();
    }

    @Test
    public void testProject() throws Exception {
        User u = em.find(User.class, CARL_CONTENT_EMAIL);
        List<Project> allProjects = new ArrayList<Project>(u.getProjects());

        TypedQuery<Project> q = em.createQuery("select t from Project t where t.owner = :user", Project.class);
        q.setParameter("user", u);
        List<Project> myProjects = q.getResultList();

        assertEquals(2, allProjects.size());
        assertEquals(u, myProjects.get(0).getOwner());
    }

    @Test(expected=RollbackException.class)
    public void testDeleteProjectConstraintViolation() throws Exception {
        // Test that we can't delete a project that has owners.
        User u = em.find(User.class, CARL_CONTENT_EMAIL);

        TypedQuery<Project> q = em.createQuery("select t from Project t where t.owner = :user", Project.class);
        q.setParameter("user", u);
        List<Project> projects = q.getResultList();

        assertEquals(1, projects.size());

        Project p = projects.get(0);

        em.getTransaction().begin();
        u.getProjects().remove(p);
        em.remove(p);
        em.getTransaction().commit();

        u = em.find(User.class, CARL_CONTENT_EMAIL);
    }

    @Test
    public void testDeleteProject() throws Exception {

        List<Project> allProjects = em.createQuery("select t from Project t", Project.class).getResultList();
        assertEquals(2, allProjects.size());

        User carl = em.find(User.class, CARL_CONTENT_EMAIL);

        TypedQuery<Project> q = em.createQuery("select t from Project t where t.owner = :user", Project.class);
        q.setParameter("user", carl);
        List<Project> carlsProjects = q.getResultList();

        Project p = carlsProjects.get(0);
        em.getTransaction().begin();
        ModelUtil.deleteProject(em, p);
        em.getTransaction().commit();

        carl = em.find(User.class, CARL_CONTENT_EMAIL);
        carlsProjects = new ArrayList<Project>(carl.getProjects());
        assertEquals(1, carlsProjects.size());

        User joe = em.find(User.class, JOE_CODER_EMAIL);
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
