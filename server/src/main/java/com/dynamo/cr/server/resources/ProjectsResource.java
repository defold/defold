package com.dynamo.cr.server.resources;

import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.proto.Config.ProjectTemplate;
import com.dynamo.cr.protocol.proto.Protocol.NewProject;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfoList;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfoList.Builder;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.git.GitRepositoryManager;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.inject.persist.Transactional;
import org.apache.commons.io.FileUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.core.Response.Status;
import java.io.File;
import java.util.List;

//NOTE: {member} isn't currently used.
//See README.md in this project for additional information
@Path("/projects/{member}")
@RolesAllowed(value = {"user"})
public class ProjectsResource extends BaseResource {

    private static final Logger LOGGER = LoggerFactory.getLogger(ProjectsResource.class);
    private final GitRepositoryManager gitRepositoryManager = new GitRepositoryManager();

    private ProjectTemplate findProjectTemplate(String id) {
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

        long projectCount = ModelUtil.getProjectCount(em, user);
        int maxProjectCount = server.getConfiguration().getMaxProjectCount();
        if (projectCount >= maxProjectCount) {
            throw new ServerException(String.format("Max number of projects (%d) has already been reached.", maxProjectCount));
        }

        Project project = ModelUtil.newProject(em, user, newProject.getName(), newProject.getDescription());
        em.flush();

        File projectPath = null;
        try {
            Configuration configuration = server.getConfiguration();
            String repositoryRoot = configuration.getRepositoryRoot();
            projectPath = new File(String.format("%s/%d", repositoryRoot, project.getId()));
            projectPath.mkdir();

            String templatePath = getTemplatePath(newProject);

            if (templatePath != null) {
                gitRepositoryManager.createRepository(projectPath, templatePath);
            } else {
                gitRepositoryManager.createRepository(projectPath);
            }
        } catch (Throwable e) {
            LOGGER.error(e.getMessage(), e);
            ModelUtil.removeProject(em, project);

            if (projectPath != null) {
                FileUtils.deleteQuietly(projectPath);
            }
            throw new ServerException("Unable to create project. Internal error.", e, Status.INTERNAL_SERVER_ERROR);
        }
        return ResourceUtil.createProjectInfo(server.getConfiguration(), user, project);
    }

    private String getTemplatePath(NewProject newProject) {
        if (newProject.hasTemplateId()) {
            ProjectTemplate projectTemplate = findProjectTemplate(newProject.getTemplateId());
            String path = projectTemplate.getPath();
            String originPath = path.replace("{cwd}", System.getProperty("user.dir"));

            File templatePath = new File(originPath);
            if (!templatePath.exists()) {
                throw new ServerException(String.format("Invalid project template path: %s", path));
            }
            return originPath;
        } else {
            return null;
        }
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
