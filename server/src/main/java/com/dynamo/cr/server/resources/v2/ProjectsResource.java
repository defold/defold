package com.dynamo.cr.server.resources.v2;

import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.server.clients.magazine.MagazineClient;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.resources.BaseResource;
import com.dynamo.cr.server.resources.mappers.ProjectSiteMapper;
import com.dynamo.cr.server.services.ProjectService;

import javax.inject.Inject;
import javax.ws.rs.GET;
import javax.ws.rs.Path;
import javax.ws.rs.QueryParam;
import java.util.List;

@Path("/v2/projects/")
public class ProjectsResource extends BaseResource {

    @Inject
    private ProjectService projectService;

    @Inject
    private MagazineClient magazineClient;

    @GET
    @Path("sites")
    public Protocol.ProjectSiteList getProjectSites(@QueryParam("limit") int limit) {
        List<Project> projects = projectService.findProjectsWithSite(limit);
        return ProjectSiteMapper.map(getUser(), projects, magazineClient);
    }
}
