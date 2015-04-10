package com.dynamo.cr.server.resources;

import java.util.List;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.GET;
import javax.ws.rs.Path;

import com.dynamo.cr.proto.Config.ProjectTemplate;
import com.dynamo.cr.protocol.proto.Protocol.ProjectTemplateInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProjectTemplateInfo.Builder;
import com.dynamo.cr.protocol.proto.Protocol.ProjectTemplateInfoList;

@Path("/repository")
@RolesAllowed(value = { "user" })
public class RepositoryResource extends BaseResource {

    @GET
    @Path("/project_templates")
    public ProjectTemplateInfoList getProjectTemplates() {
        List<ProjectTemplate> projectTemplates = server.getConfiguration().getProjectTemplatesList();
        ProjectTemplateInfoList.Builder infoListBuilder = ProjectTemplateInfoList.newBuilder();
        for (ProjectTemplate projectTemplate : projectTemplates) {
            Builder b = ProjectTemplateInfo.newBuilder()
                .setId(projectTemplate.getId())
                .setDescription(projectTemplate.getDescription());
            infoListBuilder.addTemplates(b);
        }
        return infoListBuilder.build();
    }

}
