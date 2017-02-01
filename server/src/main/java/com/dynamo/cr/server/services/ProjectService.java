package com.dynamo.cr.server.services;

import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.model.*;
import com.dynamo.inject.persist.Transactional;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import java.util.List;
import java.util.Optional;
import java.util.stream.Collectors;

public class ProjectService {
    @Inject
    private EntityManager entityManager;

    public Optional<Project> find(Long projectId) {
        return Optional.ofNullable(entityManager.find(Project.class, projectId));
    }

    public List<Project> findAll(User user) {
        return entityManager
                .createQuery("select p from Project p where :user member of p.members", Project.class)
                .setParameter("user", user)
                .getResultList();
    }

    public List<ProjectSite> findAllSites(User user) {
        List<Project> projects = findAll(user);
        return projects.stream().map(Project::getProjectSite).collect(Collectors.toList());
    }

    @Transactional
    public void addAppStoreReference(Long projectId, AppStoreReference appStoreReference) {
        getProjectSite(projectId).getAppStoreReferences().add(appStoreReference);
    }

    @Transactional
    public void deleteAppStoreReference(Long projectId, Long id) {
        ProjectSite projectSite = getProjectSite(projectId);

        Optional<AppStoreReference> any = projectSite.getAppStoreReferences().stream()
                .filter(appStoreReference -> appStoreReference.getId().equals(id))
                .findAny();

        any.ifPresent(appStoreReference -> projectSite.getAppStoreReferences().remove(appStoreReference));
    }

    @Transactional
    public void addScreenshot(Long projectId, Screenshot screenshot) {
        getProjectSite(projectId).getScreenshots().add(screenshot);
    }

    @Transactional
    public void deleteScreenshot(Long projectId, Long id) {
        ProjectSite projectSite = getProjectSite(projectId);

        Optional<Screenshot> any = projectSite.getScreenshots().stream()
                .filter(screenshot -> screenshot.getId().equals(id))
                .findAny();

        any.ifPresent(screenshot -> projectSite.getScreenshots().remove(screenshot));
    }

    public ProjectSite getProjectSite(Long projectId) {
        Project project = find(projectId)
                .orElseThrow(() -> new ServerException(String.format("No such project %s", projectId)));

        if (project.getProjectSite() == null) {
            project.setProjectSite(new ProjectSite());
        }

        entityManager.persist(project);

        return project.getProjectSite();
    }

    @Transactional
    public void updateProjectSite(Long projectId, Protocol.ProjectSite projectSite) {
        ProjectSite existingProjectSite = getProjectSite(projectId);

        if (projectSite.hasName()) {
            existingProjectSite.setName(projectSite.getName());
        }

        if (projectSite.hasDescription()) {
            existingProjectSite.setDescription(projectSite.getDescription());
        }

        if (projectSite.hasStudioName()) {
            existingProjectSite.setStudioName(projectSite.getStudioName());
        }

        if (projectSite.hasStudioUrl()) {
            existingProjectSite.setStudioUrl(projectSite.getStudioUrl());
        }

        if (projectSite.hasCoverImageUrl()) {
            existingProjectSite.setCoverImageUrl(projectSite.getCoverImageUrl());
        }

        if (projectSite.hasStoreFrontImageUrl()) {
            existingProjectSite.setStoreFrontImageUrl(projectSite.getStoreFrontImageUrl());
        }

        if (projectSite.hasDevLogUrl()) {
            existingProjectSite.setDevLogUrl(projectSite.getDevLogUrl());
        }

        if (projectSite.hasReviewUrl()) {
            existingProjectSite.setReviewUrl(projectSite.getReviewUrl());
        }
    }
}
