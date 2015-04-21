package com.dynamo.cr.server.resources;

import java.io.File;
import java.util.List;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.core.Response.Status;

import org.eclipse.jgit.api.Git;
import org.eclipse.jgit.lib.StoredConfig;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

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
import com.dynamo.inject.persist.Transactional;

//NOTE: {member} isn't currently used.
//See README.md in this project for additional information
@Path("/projects/{member}")
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
    public ProjectInfo newProject(NewProject newProject) {
        User user = getUser();

        List<Project> projects = em.createQuery("select p from Project p where :user member of p.members", Project.class).setParameter("user", user).getResultList();
        int n = 0;
        for (Project project : projects) {
            if (project.getOwner().getId() == user.getId()) {
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

            Git git = null;
            if (newProject.hasTemplateId()) {
                ProjectTemplate projectTemplate = findProjectTemplate(newProject.getTemplateId());

                String path = projectTemplate.getPath();
                path = path.replace("{cwd}", System.getProperty("user.dir"));

                File templatePath = new File(path);
                if (!templatePath.exists()) {
                    throw new ServerException(String.format("Invalid project template path: %s", projectTemplate.getPath()));
                }

                git = Git.cloneRepository()
                        .setBare(true)
                        .setURI(path)
                        .setDirectory(projectPath.getAbsoluteFile())
                        .call();
            }
            else {
                git = Git.init().setBare(true).setDirectory(projectPath.getAbsoluteFile()).call();
            }
            StoredConfig config = git.getRepository().getConfig();
            config.setBoolean("http", null, "receivepack", true);
            config.setString("core", null, "sharedRepository", "group");
            config.save();
        }
        catch (Throwable e) {
            logger.error(e.getMessage(), e);
            ModelUtil.removeProject(em, project);
            throw new ServerException("Unable to create project. Internal error.", e, Status.INTERNAL_SERVER_ERROR);
        }

        return ResourceUtil.createProjectInfo(server.getConfiguration(), user, project);
    }

    @GET
    public ProjectInfoList getProjects() {
        User user = getUser();
        List<Project> list = em.createQuery("select p from Project p where :user member of p.members", Project.class).setParameter("user", user).getResultList();

        Builder listBuilder = ProjectInfoList.newBuilder();
        for (Project project : list) {
            ProjectInfo pi = ResourceUtil.createProjectInfo(server.getConfiguration(), user, project);
            listBuilder.addProjects(pi);
        }

        return listBuilder.build();
    }

}
