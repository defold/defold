package com.dynamo.cr.server;

import java.util.HashMap;

import javax.inject.Inject;
import javax.persistence.EntityManagerFactory;

import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.eclipse.persistence.jpa.PersistenceProvider;

import com.dynamo.cr.proto.Config.Configuration;
import com.google.inject.Provider;

public class EntityManagerFactoryProvider implements Provider<EntityManagerFactory>{
    private Configuration configuration;

    @Inject
    public EntityManagerFactoryProvider(Configuration configuration) {
        this.configuration = configuration;
    }

    @Override
    public EntityManagerFactory get() {
        HashMap<String, Object> props = new HashMap<String, Object>();
        props.put(PersistenceUnitProperties.CLASSLOADER, this.getClass().getClassLoader());
        // NOTE: JPA-PersistenceUnits: in plug-in MANIFEST.MF has to be set. Otherwise the persistence unit is not found.
        EntityManagerFactory emf = new PersistenceProvider().createEntityManagerFactory(configuration.getPersistenceUnitName(), props);
        return emf;
    }
}
