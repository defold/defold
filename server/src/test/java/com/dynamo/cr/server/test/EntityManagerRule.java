package com.dynamo.cr.server.test;

import com.google.inject.Provider;
import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.eclipse.persistence.jpa.PersistenceProvider;
import org.junit.rules.ExternalResource;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import java.io.File;
import java.util.HashMap;

public class EntityManagerRule extends ExternalResource {
    private static final String PERSISTENCE_UNIT_NAME = "unit-test";
    private EntityManagerFactory factory;
    private EntityManager entityManager;

    @Override
    protected void before() throws Throwable {
        super.before();

        File tmpTestdb = new File("tmp/testdb");
        if (tmpTestdb.exists()) {
            getClass().getClassLoader().loadClass("org.apache.derby.jdbc.EmbeddedDriver");
            Util.dropAllTables();
        }

        HashMap<String, Object> properties = new HashMap<>();
        properties.put(PersistenceUnitProperties.CLASSLOADER, this.getClass().getClassLoader());

        // NOTE: JPA-PersistenceUnits: unit-test in plug-in MANIFEST.MF has to be set. Otherwise the persistence unit is not found.
        System.setProperty(PersistenceUnitProperties.ECLIPSELINK_PERSISTENCE_XML, "META-INF/test_persistence.xml");
        factory = new PersistenceProvider().createEntityManagerFactory(PERSISTENCE_UNIT_NAME, properties);

        entityManager = factory.createEntityManager();
        entityManager.getTransaction().begin();
    }

    @Override
    protected void after() {
        super.after();
        if (entityManager.getTransaction().isActive()) {
            if (entityManager.getTransaction().getRollbackOnly()) {
                entityManager.getTransaction().rollback();
            } else {
                entityManager.getTransaction().commit();
            }
        }
        entityManager.close();
        factory.close();
    }

    public EntityManager getEntityManager() {
        return entityManager;
    }

    public EntityManagerFactory getFactory() {
        return factory;
    }

    public Provider<EntityManager> getEntityManagerProvider() {
        return () -> entityManager;
    }
}
