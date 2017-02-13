package com.dynamo.cr.server.services;

import com.dynamo.cr.proto.Config;
import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.clients.magazine.MagazineClient;
import com.dynamo.cr.server.model.*;
import com.dynamo.inject.persist.Transactional;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.*;
import java.util.List;
import java.util.Optional;
import java.util.UUID;
import java.util.stream.Collectors;

public class ProjectService {
    @Inject
    private EntityManager entityManager;

    @Inject
    private Config.Configuration configuration;

    private MagazineClient magazineClient;

    private MagazineClient getMagazineClient() throws Exception {
        if (magazineClient == null) {
            magazineClient = new MagazineClient(configuration.getMagazineServiceUrl());
        }
        return magazineClient;
    }

    public Optional<Project> find(Long projectId) {
        return Optional.ofNullable(entityManager.find(Project.class, projectId));
    }

    public List<Project> findAll(User user) {
        return entityManager
                .createQuery("select p from Project p where :user member of p.members", Project.class)
                .setParameter("user", user)
                .getResultList();
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

    private ProjectSite getProjectSite(Long projectId) {
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

    @Transactional
    public void uploadPlayableFiles(String username, Long projectId, InputStream file) throws Exception {
        Path tempFile = null;

        try {
            tempFile = Files.createTempFile("defold-playable-", ".zip");
            Files.copy(file, tempFile, StandardCopyOption.REPLACE_EXISTING);

            Path playablePath = findPlayableInArchive(tempFile)
                    .orElseThrow(() -> new Exception("No playable found in zip archive."));

            uploadPlayableFiles(username, projectId, playablePath);

            ProjectSite projectSite = getProjectSite(projectId);
            projectSite.setPlayableUploaded(true);
        } finally {
            if (tempFile != null) {
                Files.deleteIfExists(tempFile);
            }
        }
    }

    private Optional<Path> findPlayableInArchive(Path zipFile) throws IOException {
        FileSystem zipFileSystem = FileSystems.newFileSystem(zipFile, null);

        for (Path rootDirectory : zipFileSystem.getRootDirectories()) {
            Optional<java.nio.file.Path> playablePath = Files
                    .find(rootDirectory, 100, (path, basicFileAttributes) -> path.endsWith("index.html"))
                    .map(Path::getParent)
                    .findAny();

            if (playablePath.isPresent()) {
                return playablePath;
            }
        }

        return Optional.empty();
    }

    private void uploadPlayableFiles(String user, long projectId, Path playablePath) throws Exception {
        String writeToken = getMagazineClient().createWriteToken(user, String.format("/projects/%d/playable", projectId));
        List<Path> playablePaths = Files.walk(playablePath).collect(Collectors.toList());

        for (Path path : playablePaths) {
            getMagazineClient().put(writeToken, playablePath.relativize(path.getParent()).toString(), path);
        }
    }

    public void addScreenshotImage(String user, long projectId, String originalFilename, InputStream file) throws Exception {
        String contextPath = String.format("/projects/%d/screenshots", projectId);
        String writeToken = getMagazineClient().createWriteToken(user, contextPath);
        String filename = UUID.randomUUID().toString() + "-" + originalFilename;
        getMagazineClient().put(writeToken, "", file, filename);

        addScreenshot(projectId, new Screenshot(Paths.get(contextPath, filename).toString(), Screenshot.MediaType.IMAGE));
    }
}
