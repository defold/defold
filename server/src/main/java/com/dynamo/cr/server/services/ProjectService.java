package com.dynamo.cr.server.services;

import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.clients.magazine.MagazineClient;
import com.dynamo.cr.server.model.*;
import com.dynamo.inject.persist.Transactional;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import javax.ws.rs.core.Response;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.*;
import java.util.List;
import java.util.Optional;
import java.util.UUID;
import java.util.stream.Collectors;

public class ProjectService {
    private static Logger LOGGER = LoggerFactory.getLogger(ProjectService.class);

    @Inject
    private EntityManager entityManager;

    @Inject
    private UserService userService;

    @Inject
    private MagazineClient magazineClient;

    public Optional<Project> find(Long projectId) {
        return Optional.ofNullable(entityManager.find(Project.class, projectId));
    }

    public List<Project> findAll(User user) {
        return entityManager
                .createQuery("select p from Project p where :user member of p.members", Project.class)
                .setParameter("user", user)
                .getResultList();
    }

    public List<Project> findProjectsWithSite(int limit) {
        return entityManager
                .createQuery("select p from Project p where p.projectSite is not null and p.projectSite.publicSite = true", Project.class)
                .setMaxResults(limit)
                .getResultList();
    }

    @Transactional
    public void addMember(Long projectId, User user, String newMemberEmail) {

        Project project = find(projectId)
                .orElseThrow(() -> new ServerException(String.format("No such project %s", projectId), Response.Status.NOT_FOUND));

        User member = userService.findByEmail(newMemberEmail)
                .orElseThrow(() -> new ServerException("User not found", Response.Status.NOT_FOUND));

        // Connect new member to owner (used in e.g. auto-completion)
        user.getConnections().add(member);

        project.getMembers().add(member);
        member.getProjects().add(project);
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
    public void deleteScreenshot(User user, Long projectId, Long id) throws Exception {
        ProjectSite projectSite = getProjectSite(projectId);

        Optional<Screenshot> any = projectSite.getScreenshots().stream()
                .filter(screenshot -> screenshot.getId().equals(id))
                .findAny();

        any.ifPresent(screenshot -> {
            projectSite.getScreenshots().remove(screenshot);
            try {
                magazineClient.delete(user.getEmail(), any.get().getUrl());
            } catch (Exception e) {
                LOGGER.warn("Failed to delete screenshot from Magazine service.", e);
            }
        });
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

        if (projectSite.hasDevLogUrl()) {
            existingProjectSite.setDevLogUrl(projectSite.getDevLogUrl());
        }

        if (projectSite.hasReviewUrl()) {
            existingProjectSite.setReviewUrl(projectSite.getReviewUrl());
        }

        if (projectSite.hasLibraryUrl()) {
            existingProjectSite.setLibraryUrl(projectSite.getLibraryUrl());
        }

        if (projectSite.hasIsPublicSite()) {
            existingProjectSite.setPublicSite(projectSite.getIsPublicSite());
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
        String writeToken = magazineClient.createWriteToken(user, String.format("/projects/%d/playable", projectId));
        List<Path> playablePaths = Files.walk(playablePath).collect(Collectors.toList());

        for (Path path : playablePaths) {
            if (path.getFileName().toString().equals("index.html")) {
                String uploadedIndexHtml = new String(Files.readAllBytes(path), "UTF-8");

                int insertionPoint = uploadedIndexHtml.indexOf("</style>");

                String htmlAddition = "\n" +
                        "      .canvas-app-container {\n" +
                        "        overflow: hidden;\n" +
                        "        text-align: center;\n" +
                        "        background: initial;\n" +
                        "      }\n" +
                        "\n" +
                        "      .canvas-app-progress {\n" +
                        "        width: 100%;\n" +
                        "      }\n" +
                        "\n" +
                        "      #canvas {\n" +
                        "        outline: none;\n" +
                        "        max-width: 100%;\n" +
                        "        background-color: black !important;\n" +
                        "      }\n" +
                        "\n" +
                        "      #fullscreen {\n" +
                        "        display: none;\n" +
                        "      }\n";

                String modifiedIndexHtml = uploadedIndexHtml.substring(0, insertionPoint) + htmlAddition + uploadedIndexHtml.substring(insertionPoint);

                magazineClient.put(writeToken, playablePath.relativize(path.getParent()).toString(),
                        new ByteArrayInputStream(modifiedIndexHtml.getBytes()), "index.html");
            } else {
                magazineClient.put(writeToken, playablePath.relativize(path.getParent()).toString(), path);
            }
        }
    }

    public void addScreenshotImage(String user, long projectId, String originalFilename, InputStream file) throws Exception {
        String resourcePath = uploadImage(projectId, user, originalFilename, file);
        addScreenshot(projectId, new Screenshot(resourcePath, Screenshot.MediaType.IMAGE));
    }


    private String uploadImage(long projectId, String user, String originalFilename, InputStream file) throws Exception {
        String contextPath = String.format("/projects/%d/images", projectId);
        return uploadFile(user, originalFilename, file, contextPath);
    }

    private String uploadAttachment(long projectId, String user, String originalFilename, InputStream file) throws Exception {
        String contextPath = String.format("/projects/%d/attachments", projectId);
        return uploadFile(user, originalFilename, file, contextPath);
    }

    private String uploadFile(String user, String originalFilename, InputStream file, String contextPath) throws Exception {
        String writeToken = magazineClient.createWriteToken(user, contextPath);
        String filename = UUID.randomUUID().toString() + "-" + originalFilename;
        magazineClient.put(writeToken, "", file, filename);
        return Paths.get(contextPath, filename).toString();
    }

    @Transactional
    public void addCoverImage(String email, Long projectId, String fileName, InputStream file) throws Exception {
        String resourcePath = uploadImage(projectId, email, fileName, file);
        getProjectSite(projectId).setCoverImageUrl(resourcePath);
    }

    @Transactional
    public void addStoreFrontImage(String email, Long projectId, String fileName, InputStream file) throws Exception {
        String resourcePath = uploadImage(projectId, email, fileName, file);
        getProjectSite(projectId).setStoreFrontImageUrl(resourcePath);
    }

    @Transactional
    public void addPlayableImage(String email, Long projectId, String fileName, InputStream file) throws Exception {
        String resourcePath = uploadImage(projectId, email, fileName, file);
        getProjectSite(projectId).setPlayableImageUrl(resourcePath);
    }

    @Transactional
    public void addAttachment(String email, Long projectId, String fileName, InputStream file) throws Exception {
        String resourcePath = uploadAttachment(projectId, email, fileName, file);
        getProjectSite(projectId).setAttachmentUrl(resourcePath);
    }

    public boolean isProjectMember(User user, Project project) {
        return user != null && project.getMembers().stream().anyMatch(member -> member.getEmail().equals(user.getEmail()));
    }
}
