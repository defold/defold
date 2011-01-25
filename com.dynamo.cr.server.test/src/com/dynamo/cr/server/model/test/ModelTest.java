package com.dynamo.cr.server.model.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Set;

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
    static final String LISA_USER_EMAIL = "lisa.user@gmail.com";
    private static final String PERSISTENCE_UNIT_NAME = "unit-test";
    private static EntityManagerFactory factory;
    private EntityManager em;

    private void dropAllTables() throws ClassNotFoundException, SQLException {
        getClass().getClassLoader().loadClass("org.apache.derby.jdbc.EmbeddedDriver");
        Connection con = DriverManager.getConnection("jdbc:derby:tmp/testdb");
        ResultSet rs = con.getMetaData().getTables(null, null, null, new String[] {"TABLE"});
        ArrayList<String> queries = new ArrayList<String>();
        while (rs.next()) {
            String q = "DROP TABLE " + rs.getString("TABLE_SCHEM") + "." + rs.getString("TABLE_NAME");
            queries.add(q);
        }

        // Brute force drop of all tables. We don't know the correct drop order due to constraints
        // iterate until done
        int iter = 0;
        while (queries.size() > 0) {
            String q = queries.remove(0);
            Statement stmnt = con.createStatement();
            try {
                stmnt.execute(q);
            }
            catch (SQLException e) {
                // Failed to drop. Add last in query list
                queries.add(q);
            }
            finally {
                stmnt.close();
            }
            if (iter > 100) {
                throw new RuntimeException(String.format("Unable to drop all tables after %d iterations. Something went very wrong", iter));
            }
        }
        con.close();
    }

    @Before
    public void setUp() throws Exception {

        // "drop-and-create-tables" can't handle model changes correctly. We need to drop all tables first.
        // Eclipse-link only drops tables currently specified. When the model change the table set also change.
        dropAllTables();

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

        User u3 = new User();
        u3.setEmail(LISA_USER_EMAIL);
        u3.setFirstName("Lisa");
        u3.setLastName("User");
        em.persist(u3);

        // Create new projects
        Project p1 = ModelUtil.newProject(em, u1, "Carl Contents Project");
        Project p2 = ModelUtil.newProject(em, u2, "Joe Coders' Project");

        // Add users to project
        u2.getProjects().add(p1);
        u1.getProjects().add(p2);
        u3.getProjects().add(p1);
        u3.getProjects().add(p2);

        p1.getUsers().add(u2);
        p2.getUsers().add(u1);
        p1.getUsers().add(u3);
        p2.getUsers().add(u3);

        em.persist(u1);
        em.persist(u2);
        em.persist(u3);

        em.getTransaction().commit();
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

    @Test(expected=RollbackException.class)
    public void createInvalidUser() {
        // null is not a valid first name
        em.getTransaction().begin();

        User u1 = new User();
        u1.setEmail("foo@bar.com");
        u1.setFirstName(null);
        u1.setLastName("Content");
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
    public void testRemoveProject() throws Exception {

        List<Project> allProjects = em.createQuery("select t from Project t", Project.class).getResultList();
        assertEquals(2, allProjects.size());

        User carl = ModelUtil.findUserByEmail(em, CARL_CONTENT_EMAIL);

        TypedQuery<Project> q = em.createQuery("select t from Project t where t.owner = :user", Project.class);
        q.setParameter("user", carl);
        List<Project> carlsProjects = q.getResultList();

        Project p = carlsProjects.get(0);
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
}
