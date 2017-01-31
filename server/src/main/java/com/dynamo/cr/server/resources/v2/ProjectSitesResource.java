package com.dynamo.cr.server.resources.v2;

import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.model.AppStoreReference;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.Screenshot;
import com.dynamo.cr.server.resources.BaseResource;
import com.dynamo.cr.server.resources.mappers.ProjectSiteMapper;
import com.dynamo.cr.server.services.ProjectService;

import javax.annotation.security.RolesAllowed;
import javax.inject.Inject;
import javax.ws.rs.*;
import java.util.List;

@Path("/v2/projects/{project}/site/")
public class ProjectSitesResource extends BaseResource {

    @Inject
    private ProjectService projectService;

    @GET
    @Path("sites")
    @RolesAllowed(value = {"member"})
    public Protocol.ProjectSiteList getProjectSites() {
        List<Project> projects = projectService.findAll(getUser());
        return ProjectSiteMapper.map(projects);
    }

    @GET
    @RolesAllowed(value = {"member"})
    public Protocol.ProjectSite getProjectSite(@PathParam("project") Long projectId) {
        Project project = projectService.find(projectId)
                .orElseThrow(() -> new ServerException(String.format("No such project %s", projectId)));

        return ProjectSiteMapper.map(project);
    }

    @PUT
    @RolesAllowed(value = {"owner"})
    public void updateProjectSite(@PathParam("project") Long projectId, Protocol.ProjectSite projectSite) {
        projectService.updateProjectSite(projectId, projectSite);
    }

    @POST
    @RolesAllowed(value = {"owner"})
    @Path("app_store_references")
    public void addAppStoreReference(@PathParam("project") Long projectId,
                                     Protocol.NewAppStoreReference newAppStoreReference) {
        projectService.addAppStoreReference(
                projectId,
                new AppStoreReference(newAppStoreReference.getLabel(), newAppStoreReference.getUrl()));
    }

    @DELETE
    @RolesAllowed(value = {"owner"})
    @Path("app_store_references/{appStoreReferenceId}")
    public void deleteAppStoreReference(@PathParam("project") Long projectId, @PathParam("appStoreReferenceId") Long id) {
        projectService.deleteAppStoreReference(projectId, id);
    }

    @POST
    @RolesAllowed(value = {"owner"})
    @Path("screenshots")
    public void addScreenshot(@PathParam("project") Long projectId, Protocol.NewScreenshot newScreenshot) {
        projectService.addScreenshot(
                projectId,
                new Screenshot(newScreenshot.getUrl(), Screenshot.MediaType.valueOf(newScreenshot.getMediaType().name())));
    }

    @DELETE
    @RolesAllowed(value = {"owner"})
    @Path("screenshots/{screenshotId}")
    public void deleteScreenshot(@PathParam("project") Long projectId, @PathParam("screenshotId") Long id) {
        projectService.deleteScreenshot(projectId, id);
    }
}
