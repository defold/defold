package com.dynamo.cr.server.model.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import java.util.HashMap;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.persistence.RollbackException;

import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.eclipse.persistence.jpa.osgi.PersistenceProvider;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

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
        em.close();
        // NOTE: Important to close the factory if the tables shoudl be dropped and created in setUp above (drop-and-create-tables in persistance.xml)
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

}
