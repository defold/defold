package com.dynamo.cr.server;

import java.util.regex.Pattern;

import javax.persistence.EntityManager;

import com.dynamo.cr.branchrepo.BranchRepository;
import com.dynamo.cr.server.model.User;

public class RemoteBranchRepository extends BranchRepository {

    private Server server;

    public RemoteBranchRepository(Server server, String branchRoot, String repositoryRoot,
            String builtinsDirectory, Pattern[] filterPatterns) {
        super(branchRoot, repositoryRoot, builtinsDirectory, filterPatterns);
        this.server = server;
    }

    @Override
    protected String getFullName(String userId) {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        User u;
        try {
            u = server.getUser(em, userId);
        } catch (ServerException e) {
            throw new RuntimeException(e);
        } finally {
            em.close();
        }
        String fullName = u.getFirstName();
        if (!u.getLastName().isEmpty()) {
            fullName = String.format("%s %s", fullName, u.getLastName());
        }
        return fullName;
    }

    @Override
    protected String getEmail(String userId) {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        User u;
        try {
            u = server.getUser(em, userId);
        } catch (ServerException e) {
            throw new RuntimeException(e);
        } finally {
            em.close();
        }
        return u.getEmail();
    }

    @Override
    protected void ensureProject(String project) {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        try {
            server.getProject(em, project);
        } catch (ServerException e) {
            throw new RuntimeException("Invalid project " + project, e);
        } finally {
            em.close();
        }
    }

}
