package com.dynamo.cr.server.resources.v2;

import com.dynamo.cr.proto.Config;
import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.clients.magazine.MagazineClient;
import com.dynamo.cr.server.model.AppStoreReference;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.Screenshot;
import com.dynamo.cr.server.resources.BaseResource;
import com.dynamo.cr.server.resources.mappers.ProjectSiteMapper;
import com.dynamo.cr.server.services.ProjectService;
import com.sun.jersey.core.header.FormDataContentDisposition;
import com.sun.jersey.multipart.FormDataParam;

import javax.annotation.security.RolesAllowed;
import javax.inject.Inject;
import javax.ws.rs.*;
import javax.ws.rs.core.MediaType;
import java.io.InputStream;
import java.util.List;

@Path("/v2/projects/{project}/site/")
public class ProjectSitesResource extends BaseResource {

    @Inject
    private ProjectService projectService;

    @Inject
    private Config.Configuration configuration;

    private MagazineClient magazineClient;

    @GET
    @Path("sites")
    @RolesAllowed(value = {"member"})
    public Protocol.ProjectSiteList getProjectSites() throws Exception {
        List<Project> projects = projectService.findAll(getUser());
        return ProjectSiteMapper.map(getUser(), projects, getMagazineClient());
    }

    @GET
    public Protocol.ProjectSite getProjectSite(@PathParam("project") Long projectId) throws Exception {
        Project project = projectService.find(projectId)
                .orElseThrow(() -> new ServerException(String.format("No such project %s", projectId)));

        return ProjectSiteMapper.map(getUser(), project, getMagazineClient());
    }

    private MagazineClient getMagazineClient() throws Exception {
        if (magazineClient == null) {
            magazineClient = new MagazineClient(configuration.getMagazineServiceUrl());
        }
        return magazineClient;
    }

    @PUT
    @RolesAllowed(value = {"member"})
    public void updateProjectSite(@PathParam("project") Long projectId, Protocol.ProjectSite projectSite) {
        projectService.updateProjectSite(projectId, projectSite);
    }

    @POST
    @RolesAllowed(value = {"member"})
    @Path("app_store_references")
    public void addAppStoreReference(@PathParam("project") Long projectId,
                                     Protocol.NewAppStoreReference newAppStoreReference) {
        projectService.addAppStoreReference(
                projectId,
                new AppStoreReference(newAppStoreReference.getLabel(), newAppStoreReference.getUrl()));
    }

    @DELETE
    @RolesAllowed(value = {"member"})
    @Path("app_store_references/{appStoreReferenceId}")
    public void deleteAppStoreReference(@PathParam("project") Long projectId, @PathParam("appStoreReferenceId") Long id) {
        projectService.deleteAppStoreReference(projectId, id);
    }

    @POST
    @RolesAllowed(value = {"member"})
    @Path("screenshots")
    public void addScreenshot(@PathParam("project") Long projectId, Protocol.NewScreenshot newScreenshot) {
        projectService.addScreenshot(
                projectId,
                new Screenshot(newScreenshot.getUrl(), Screenshot.MediaType.valueOf(newScreenshot.getMediaType().name())));
    }

    @POST
    @RolesAllowed(value = {"member"})
    @Path("screenshots/images")
    @Consumes(MediaType.MULTIPART_FORM_DATA)
    public void addScreenshotImage(@PathParam("project") Long projectId,
                                   @FormDataParam("file") InputStream file,
                                   @FormDataParam("file") FormDataContentDisposition fileInfo) throws Exception {
        projectService.addScreenshotImage(getUser().getEmail(), projectId, fileInfo.getFileName(), file);
    }

    @DELETE
    @RolesAllowed(value = {"member"})
    @Path("screenshots/{screenshotId}")
    public void deleteScreenshot(@PathParam("project") Long projectId, @PathParam("screenshotId") Long id) throws Exception {
        projectService.deleteScreenshot(getUser(), projectId, id);
    }

    @PUT
    @RolesAllowed(value = {"member"})
    @Path("playable")
    @Consumes(MediaType.MULTIPART_FORM_DATA)
    public void putPlayable(@PathParam("project") Long projectId,
                            @FormDataParam("file") InputStream file,
                            @FormDataParam("file") FormDataContentDisposition fileInfo) throws Exception {
        projectService.uploadPlayableFiles(getUser().getEmail(), projectId, file);
    }

    @POST
    @RolesAllowed(value = {"member"})
    @Path("cover_image")
    @Consumes(MediaType.MULTIPART_FORM_DATA)
    public void addCoverImage(@PathParam("project") Long projectId,
                                   @FormDataParam("file") InputStream file,
                                   @FormDataParam("file") FormDataContentDisposition fileInfo) throws Exception {
        projectService.addCoverImage(getUser().getEmail(), projectId, fileInfo.getFileName(), file);
    }

    @POST
    @RolesAllowed(value = {"member"})
    @Path("store_front_image")
    @Consumes(MediaType.MULTIPART_FORM_DATA)
    public void addStoreFrontImage(@PathParam("project") Long projectId,
                              @FormDataParam("file") InputStream file,
                              @FormDataParam("file") FormDataContentDisposition fileInfo) throws Exception {
        projectService.addStoreFrontImage(getUser().getEmail(), projectId, fileInfo.getFileName(), file);
    }

    @POST
    @RolesAllowed(value = {"member"})
    @Path("playable_image")
    @Consumes(MediaType.MULTIPART_FORM_DATA)
    public void addPlayableImage(@PathParam("project") Long projectId,
                              @FormDataParam("file") InputStream file,
                              @FormDataParam("file") FormDataContentDisposition fileInfo) throws Exception {
        projectService.addPlayableImage(getUser().getEmail(), projectId, fileInfo.getFileName(), file);
    }
}
