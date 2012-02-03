package com.dynamo.cr.server.resources;

import java.io.File;
import java.util.List;

import javax.annotation.security.RolesAllowed;
import javax.persistence.EntityManager;
import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.core.Response.Status;

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
import com.dynamo.server.git.GitFactory;
import com.dynamo.server.git.GitFactory.Type;
import com.dynamo.server.git.IGit;

@Path("/projects/{user}")
@RolesAllowed(value = { "user" })
public class ProjectsResource extends BaseResource {

    protected static Logger logger = LoggerFactory.getLogger(ProjectsResource.class);

    ProjectTemplate findProjectTemplate(String id) throws ServerException {
        List<ProjectTemplate> lst = server.getConfiguration().getProjectTemplatesList();
        for (ProjectTemplate projectTemplate : lst) {
            if (projectTemplate.getId().equals(id))
                return projectTemplate;
        }
        throw new ServerException(String.format("Invalid project template '%s'", id));
    }

    @POST
    public ProjectInfo newProject(@PathParam("user") String userId, NewProject newProject) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();

        User user = server.getUser(em, userId);
        em.getTransaction().begin();
        Project project = ModelUtil.newProject(em, user, newProject.getName(), newProject.getDescription());
        em.getTransaction().commit();

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

                File templatePath = new File(projectTemplate.getPath());
                if (!templatePath.exists()) {
                    throw new ServerException(String.format("Invalid project template path: %s", projectTemplate.getPath()));
                }
                git.cloneRepoBare(projectTemplate.getPath(), projectPath.getAbsolutePath(), group);
            }
            else {
                git.initBare(projectPath.getAbsolutePath(), group);
            }
            git.config(projectPath.getAbsolutePath(), "http.receivepack", "true");
        }
        catch (Throwable e) {
            logger.error(e.getMessage(), e);
            em.getTransaction().begin();
            ModelUtil.removeProject(em, project);
            em.getTransaction().commit();
            em.close();
            throw new ServerException("Unable to create project. Internal error.", Status.INTERNAL_SERVER_ERROR);
        }
        em.close();

        return ResourceUtil.createProjectInfo(server.getConfiguration(), project);
    }

    @GET
    public ProjectInfoList getProjects(@PathParam("user") String userId) throws ServerException {
        EntityManager em = server.getEntityManagerFactory().createEntityManager();
        User user = server.getUser(em, userId);

        List<Project> list = em.createQuery("select p from Project p where :user member of p.members", Project.class).setParameter("user", user).getResultList();

        Builder listBuilder = ProjectInfoList.newBuilder();
        for (Project project : list) {
            ProjectInfo pi = ResourceUtil.createProjectInfo(server.getConfiguration(), project);
            listBuilder.addProjects(pi);
        }

        return listBuilder.build();
    }

}
