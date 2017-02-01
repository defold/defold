package com.dynamo.cr.server.resources.v2;

import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.server.resources.AbstractResourceTest;
import com.dynamo.cr.server.test.TestUser;
import com.dynamo.cr.server.test.TestUtils;
import com.sun.jersey.api.client.WebResource;
import org.junit.BeforeClass;
import org.junit.Test;

import javax.persistence.EntityManager;
import java.io.IOException;

import static org.junit.Assert.assertEquals;

public class ProjectSitesResourceTest extends AbstractResourceTest {

    @BeforeClass
    public static void beforeClass() throws IOException {
        execCommand("scripts/setup_template_project.sh", "proj1");

        EntityManager entityManager = emf.createEntityManager();
        entityManager.getTransaction().begin();
        TestUtils.createTestUsers(entityManager);
        entityManager.getTransaction().commit();
    }

    private Protocol.ProjectInfo createProject(TestUser owner) {
        WebResource baseResource = createBaseResource(owner.email, owner.password);

        Protocol.NewProject newProject = Protocol.NewProject.newBuilder()
                .setName("name")
                .setDescription("description")
                .setTemplateId("proj1")
                .build();

        return baseResource.path("/projects/-1/").post(Protocol.ProjectInfo.class, newProject);
    }

    private void updateProjectSite(TestUser testUser, Long projectId, Protocol.ProjectSite projectSite) {
        WebResource baseResource = createBaseResource(testUser.email, testUser.password);
        baseResource.path("v2/projects").path(projectId.toString()).path("site").put(projectSite);
    }

    private Protocol.ProjectSite getProjectSite(TestUser testUser, Long projectId) {
        WebResource baseResource = createBaseResource(testUser.email, testUser.password);
        return baseResource.path("v2/projects").path(projectId.toString()).path("site").get(Protocol.ProjectSite.class);
    }

    private void addAppStoreReference(TestUser testUser, Long projectId, Protocol.NewAppStoreReference newAppStoreReference) {
        WebResource baseResource = createBaseResource(testUser.email, testUser.password);
        baseResource.path("v2/projects/").path(projectId.toString()).path("site/app_store_references").post(newAppStoreReference);
    }

    private void deleteAppStoreReference(TestUser testUser, Long projectId, Long appStoreReferenceId) {
        WebResource baseResource = createBaseResource(testUser.email, testUser.password);
        baseResource.path("v2/projects/").path(projectId.toString()).path("site/app_store_references").path(appStoreReferenceId.toString()).delete();
    }

    private void addScreenshot(TestUser testUser, Long projectId, Protocol.NewScreenshot newScreenshot) {
        WebResource baseResource = createBaseResource(testUser.email, testUser.password);
        baseResource.path("v2/projects/").path(projectId.toString()).path("site/screenshots").post(newScreenshot);
    }

    private void deleteScreenshot(TestUser testUser, Long projectId, Long screenshotId) {
        WebResource baseResource = createBaseResource(testUser.email, testUser.password);
        baseResource.path("v2/projects/").path(projectId.toString()).path("site/screenshots").path(screenshotId.toString()).delete();
    }

    @Test
    public void updateProjectSite() {

        Protocol.ProjectInfo project = createProject(TestUser.JAMES);

        Protocol.ProjectSite projectSite = Protocol.ProjectSite.newBuilder()
                .setName("name")
                .setDescription("description")
                .setStudioUrl("studioUrl")
                .setStudioName("studioName")
                .setCoverImageUrl("coverImageUrl")
                .setStoreFrontImageUrl("storeFrontImageUrl")
                .setDevLogUrl("devLogUrl")
                .setReviewUrl("reviewUrl")
                .build();

        updateProjectSite(TestUser.JAMES, project.getId(), projectSite);

        Protocol.ProjectSite result = getProjectSite(TestUser.JAMES, project.getId());

        assertEquals("name", result.getName());
        assertEquals("description", result.getDescription());
        assertEquals("studioUrl", result.getStudioUrl());
        assertEquals("studioName", result.getStudioName());
        assertEquals("coverImageUrl", result.getCoverImageUrl());
        assertEquals("storeFrontImageUrl", result.getStoreFrontImageUrl());
        assertEquals("devLogUrl", result.getDevLogUrl());
        assertEquals("reviewUrl", result.getReviewUrl());
        assertEquals(project.getId(), result.getProjectId());
    }

    @Test
    public void updateAppStoreReferences() {

        Protocol.ProjectInfo project = createProject(TestUser.JAMES);

        // Add two app store references.
        Protocol.NewAppStoreReference appStoreReference = Protocol.NewAppStoreReference.newBuilder()
                .setLabel("App Store")
                .setUrl("http://www.appstore.com")
                .build();
        addAppStoreReference(TestUser.JAMES, project.getId(), appStoreReference);

        Protocol.NewAppStoreReference appStoreReference2 = Protocol.NewAppStoreReference.newBuilder()
                .setLabel("Google Play")
                .setUrl("http://www.play.com")
                .build();
        addAppStoreReference(TestUser.JAMES, project.getId(), appStoreReference2);

        // Ensure that you get two references back.
        Protocol.ProjectSite projectSite = getProjectSite(TestUser.JAMES, project.getId());
        assertEquals(2, projectSite.getAppStoreReferencesCount());

        // Delete one reference.
        long id = projectSite.getAppStoreReferences(0).getId();
        deleteAppStoreReference(TestUser.JAMES, project.getId(), id);

        // Ensure that you get one reference back.
        projectSite = getProjectSite(TestUser.JAMES, project.getId());
        assertEquals(1, projectSite.getAppStoreReferencesCount());
    }

    @Test
    public void updateScreenshot() {

        Protocol.ProjectInfo project = createProject(TestUser.JAMES);

        // Add two screenshots.
        Protocol.NewScreenshot screenshot = Protocol.NewScreenshot.newBuilder()
                .setUrl("http://www.images.com")
                .setMediaType(Protocol.ScreenshotMediaType.IMAGE)
                .build();
        addScreenshot(TestUser.JAMES, project.getId(), screenshot);

        Protocol.NewScreenshot screenshot2 = Protocol.NewScreenshot.newBuilder()
                .setUrl("http://www.images.com/2")
                .setMediaType(Protocol.ScreenshotMediaType.YOUTUBE)
                .build();
        addScreenshot(TestUser.JAMES, project.getId(), screenshot2);

        // Ensure that you get two screenshots back.
        Protocol.ProjectSite projectSite = getProjectSite(TestUser.JAMES, project.getId());
        assertEquals(2, projectSite.getScreenshotsCount());

        // Delete one screenshot.
        long id = projectSite.getScreenshots(0).getId();
        deleteScreenshot(TestUser.JAMES, project.getId(), id);

        // Ensure that you get one reference back.
        projectSite = getProjectSite(TestUser.JAMES, project.getId());
        assertEquals(1, projectSite.getScreenshotsCount());
    }
}
