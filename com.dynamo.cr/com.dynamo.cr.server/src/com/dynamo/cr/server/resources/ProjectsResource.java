package com.dynamo.cr.server.resources;

import java.io.File;
import java.util.List;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.core.Response.Status;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.proto.Config.BillingProduct;
import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.proto.Config.ProjectTemplate;
import com.dynamo.cr.protocol.proto.Protocol.NewProject;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfoList;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfoList.Builder;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.templates.Templates;
import com.dynamo.inject.persist.Transactional;
import com.dynamo.server.dgit.GitFactory;
import com.dynamo.server.dgit.GitFactory.Type;
import com.dynamo.server.dgit.IGit;

@Path("/projects/{user}")
@RolesAllowed(value = { "user" })
public class ProjectsResource extends BaseResource {

    protected static Logger logger = LoggerFactory.getLogger(ProjectsResource.class);

    ProjectTemplate findProjectTemplate(String id) {
        List<ProjectTemplate> lst = server.getConfiguration().getProjectTemplatesList();
        for (ProjectTemplate projectTemplate : lst) {
            if (projectTemplate.getId().equals(id))
                return projectTemplate;
        }
        throw new ServerException(String.format("Invalid project template '%s'", id));
    }

    @POST
    @Transactional
    public ProjectInfo newProject(@PathParam("user") String userId, NewProject newProject) {
        User user = server.getUser(em, userId);

        ProjectInfoList list = getProjects(userId);
        int n = 0;
        for (ProjectInfo projectInfo : list.getProjectsList()) {
            if (projectInfo.getOwner().getId() == user.getId()) {
                // Count only projects the user own
                ++n;
            }
        }
        int maxProjectCount = server.getConfiguration().getMaxProjectCount();
        if (maxProjectCount <= n) {
            throw new ServerException(String.format("Max number of projects (%d) has already been reached.", maxProjectCount));
        }

        Project project = ModelUtil.newProject(em, user, newProject.getName(), newProject.getDescription());
        em.flush();

        try {
            Configuration configuration = server.getConfiguration();
            String repositoryRoot = configuration.getRepositoryRoot();
            File projectPath = new File(String.format("%s/%d", repositoryRoot, project.getId()));
            projectPath.mkdir();
            IGit git = GitFactory.create(Type.CGIT);
            String group = null;
            if (configuration.hasRepositoryRoot())
                group = configuration.getRepositoryGroup();

            if (newProject.hasTemplateId()) {
                ProjectTemplate projectTemplate = findProjectTemplate(newProject.getTemplateId());

                // NOTE: A bit strange that we substitute variables here. We should perhaps not
                // expose the actual configuration file represented on disk and instead have some more
                // abstract run-time structure in the server?
                String templateRoot = Templates.getDefault().getTemplateRoot();
                String path = projectTemplate.getPath();
                path = path.replace("{templates_root}", templateRoot);

                File templatePath = new File(path);
                if (!templatePath.exists()) {
                    throw new ServerException(String.format("Invalid project template path: %s", projectTemplate.getPath()));
                }
                git.cloneRepoBare(path, projectPath.getAbsolutePath(), group);
            }
            else {
                git.initBare(projectPath.getAbsolutePath(), group);
            }
            git.config(projectPath.getAbsolutePath(), "http.receivepack", "true");
        }
        catch (Throwable e) {
            logger.error(e.getMessage(), e);
            ModelUtil.removeProject(em, project);
            throw new ServerException("Unable to create project. Internal error.", e, Status.INTERNAL_SERVER_ERROR);
        }

        return ResourceUtil.createProjectInfo(server.getConfiguration(), user, project,
                ModelUtil.isMemberQualified(em, user, project, server.getConfiguration().getProductsList()));
    }

    @GET
    public ProjectInfoList getProjects(@PathParam("user") String userId) {
        User user = server.getUser(em, userId);

        List<Project> list = em.createQuery("select p from Project p where :user member of p.members", Project.class).setParameter("user", user).getResultList();

        List<BillingProduct> products = server.getConfiguration().getProductsList();
        Builder listBuilder = ProjectInfoList.newBuilder();
        for (Project project : list) {
            boolean isQualified = ModelUtil.isMemberQualified(em, user, project, products);
            ProjectInfo pi = ResourceUtil.createProjectInfo(server.getConfiguration(), user, project, isQualified);
            listBuilder.addProjects(pi);
        }

        return listBuilder.build();
    }

}
