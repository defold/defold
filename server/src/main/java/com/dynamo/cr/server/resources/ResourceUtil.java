package com.dynamo.cr.server.resources;

import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.util.TrackingID;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.eclipse.jgit.util.StringUtils;

import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;
import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;

public class ResourceUtil {

    static void throwWebApplicationException(Status status, String msg) {
        Response response = Response
                .status(status)
                .type(MediaType.TEXT_PLAIN)
                .entity(msg)
                .build();
        throw new WebApplicationException(response);
    }

    private static UserInfo createUserInfo(User user) {
        return UserInfo.newBuilder()
            .setId(user.getId())
            .setEmail(user.getEmail())
            .setFirstName(user.getFirstName())
            .setLastName(user.getLastName())
            .build();
    }

    /*
     * TODO: We should perhaps move all configuration that requires logic
     * to a setup-phase in Server? Same for getGitBaseUri below
     * Currently Configuration is a immutable configuration data-structure (protobuf)
     * We should perhaps derive settings to a new class with git-base-path etc
     */
    public static String getGitBasePath(Configuration configuration) {
        String repositoryRoot = FilenameUtils.normalize(configuration.getRepositoryRoot(), true);
        List<String> repositoryRootList = Arrays.asList(repositoryRoot.split("/"));
        if (repositoryRootList.size() < 1) {
            throw new RuntimeException("repositoryRoot path must contain at least 1 element");
        } else if (repositoryRootList.size() == 1) {
            repositoryRootList.add(0, ".");
        }

        String basePath = StringUtils.join(repositoryRootList.subList(0, repositoryRootList.size()-1), "/");
        basePath = new File(basePath).getAbsolutePath();
        return basePath;
    }

    public static String getGitBaseUri(Configuration configuration) {
        String repositoryRoot = FilenameUtils.normalize(configuration.getRepositoryRoot(), true);
        List<String> repositoryRootList = Arrays.asList(repositoryRoot.split("/"));
        if (repositoryRootList.size() < 1) {
            throw new RuntimeException("repositoryRoot path must contain at least 1 element");
        } else if (repositoryRootList.size() == 1) {
            repositoryRootList.add(0, ".");
        }

        return "/" + repositoryRootList.get(repositoryRootList.size()-1);
    }

    static ProjectInfo createProjectInfo(Configuration configuration, User user, Project project) {
        ProjectInfo.Builder b = ProjectInfo.newBuilder()
            .setId(project.getId())
            .setName(project.getName())
            .setDescription(project.getDescription())
            .setOwner(createUserInfo(project.getOwner()))
            .setCreated(project.getCreated().getTime())
            .setLastUpdated(project.getLastUpdated().getTime())
            .setRepositoryUrl(String.format("http://%s:%d%s/%d", configuration.getHostname(),
                    configuration.getGitPort(),
                    getGitBaseUri(configuration),
                    project.getId()))
            .setTrackingId(TrackingID.trackingID((int) (project.getId() & 0xffffffff)));

        if (Server.getEngineFile(configuration, Long.toString(project.getId()), "ios").exists()) {
            String key = Server.getEngineDownloadKey(project);
            String url = String.format("https://%s/projects/%d/%d/engine_manifest/ios/%s",
                    configuration.getHostname(),
                    user.getId(),
                    project.getId(),
                    key);

            b.setIOSExecutableUrl(url);
        }

        for (User u : project.getMembers()) {
            b.addMembers(createUserInfo(u));
        }

        return b.build();
    }

    static void deleteProjectRepo(Project project, Configuration configuration) throws IOException {
        // Delete git repo
        String repositoryRoot = configuration.getRepositoryRoot();
        File projectPath = new File(String.format("%s/%d", repositoryRoot, project.getId()));
        FileUtils.deleteDirectory(projectPath);
    }
}
