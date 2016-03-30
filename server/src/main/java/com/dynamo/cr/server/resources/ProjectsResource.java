package com.dynamo.cr.server.resources;

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
import com.google.common.io.Files;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.filefilter.DirectoryFileFilter;
import org.apache.commons.io.filefilter.FalseFileFilter;
import org.eclipse.jgit.api.Git;
import org.eclipse.jgit.api.errors.GitAPIException;
import org.eclipse.jgit.lib.StoredConfig;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.annotation.security.RolesAllowed;
import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.core.Response.Status;
import java.io.File;
import java.io.IOException;
import java.util.Collection;
import java.util.List;

//NOTE: {member} isn't currently used.
//See README.md in this project for additional information
@Path("/projects/{member}")
@RolesAllowed(value = {"user"})
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

            Git git = null;
            String templatePath = getTemplatePath(newProject);

            if (templatePath != null) {
                git = cloneWithoutHistory(projectPath, templatePath);
            } else {
                git = Git.init().setBare(true).setDirectory(projectPath.getAbsoluteFile()).call();
            }
            updateGitConfig(git, templatePath);

        } catch (Throwable e) {
            logger.error(e.getMessage(), e);
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

    private Git cloneWithoutHistory(File projectPath, String originPath)
            throws GitAPIException, IllegalStateException, IOException {

        File tempDir = Files.createTempDir();
        try {
            Git.cloneRepository()
                    .setURI(originPath)
                    .setDirectory(tempDir.getAbsoluteFile())
                    .call();

            // Re-init the git repository, all history will be removed
            removeGitData(tempDir);
            Git git = Git.init().setDirectory(tempDir.getAbsoluteFile()).call();

            // Add all files to new baseline
            git.add().addFilepattern(".").call();
            git.commit().setMessage("Initial commit").call();

            // Bare clone to final destination
            return Git.cloneRepository()
                    .setBare(true)
                    .setURI(tempDir.getAbsolutePath())
                    .setDirectory(projectPath.getAbsoluteFile())
                    .call();
        } finally {
            FileUtils.deleteQuietly(tempDir);
        }
    }

    private void removeGitData(File directory) throws IOException {
        // List directories
        Collection<File> directories = FileUtils.listFilesAndDirs(directory, FalseFileFilter.INSTANCE, DirectoryFileFilter.DIRECTORY);
        // Remove .git directory
        for (File dir : directories) {
            if (dir.getName().equals(".git")) {
                FileUtils.deleteDirectory(dir);
            }
        }
    }

    private void updateGitConfig(Git git, String templatePath) throws IOException {
        StoredConfig config = git.getRepository().getConfig();
        if (templatePath != null) {
            config.setString("remote", "origin", "url", templatePath);
        }
        config.setBoolean("http", null, "receivepack", true);
        config.setString("core", null, "sharedRepository", "group");
        config.save();
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
