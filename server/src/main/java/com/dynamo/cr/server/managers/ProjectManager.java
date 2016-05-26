package com.dynamo.cr.server.managers;

import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.resources.ResourceUtil;
import com.sun.jersey.api.NotFoundException;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.Response;
import java.io.IOException;
import java.util.Objects;
import java.util.Optional;

public class ProjectManager {

    private final EntityManager entityManager;

    @Inject
    public ProjectManager(EntityManager entityManager) {
        this.entityManager = entityManager;
    }

    public Optional<Project> getProject(Long projectId) {
        return Optional.ofNullable(entityManager.find(Project.class, projectId));
    }

    public void removeProject(Long projectId, String repositoryRoot) {
        Project project = getProject(projectId)
                .orElseThrow(() -> new ServerException("Project not found.", Response.Status.NOT_FOUND));

        entityManager.remove(project);

        try {
            ResourceUtil.deleteProjectRepo(project.getId(), repositoryRoot);
        } catch (IOException e) {
            throw new ServerException(String.format("Could not delete git repo for project %s", project.getName()), Response.Status.INTERNAL_SERVER_ERROR);
        }
    }

    public void addMember(Long projectId, String memberEmail, User user) {
        User member = ModelUtil.findUserByEmail(entityManager, memberEmail);
        if (member == null) {
            throw new ServerException("User not found", Response.Status.NOT_FOUND);
        }

        // Connect new member to owner (used in e.g. auto-completion)
        user.getConnections().add(member);

        Project project = getProject(projectId)
                .orElseThrow(() -> new ServerException(String.format(
                        "Failed to retrieve project %d", projectId), Response.Status.INTERNAL_SERVER_ERROR)
                );

        project.getMembers().add(member);
        member.getProjects().add(project);
        entityManager.persist(project);
        entityManager.persist(user);
        entityManager.persist(member);
    }

    public void removeMember(Long projectId, User member, User administrator) {
        Project project = getProject(projectId)
                .orElseThrow(() -> new ServerException("Project not found.", Response.Status.NOT_FOUND));

        if (!project.getMembers().contains(member)) {
            throw new NotFoundException(String.format("User %s is not a member of project %s", administrator.getId(), projectId));
        }
        // Only owners can remove other users and only non-owners can remove themselves
        boolean isOwner = Objects.equals(administrator.getId(), project.getOwner().getId());
        boolean removeSelf = Objects.equals(administrator.getId(), member.getId());
        if ((isOwner && removeSelf) || (!isOwner && !removeSelf)) {
            throw new WebApplicationException(403);
        }

        ModelUtil.removeMember(project, member);
        entityManager.persist(project);
        entityManager.persist(member);
    }
}
