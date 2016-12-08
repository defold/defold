package com.dynamo.cr.server.resources.mappers;

import com.dynamo.cr.proto.Config;
import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.resources.ResourceUtil;
import com.dynamo.cr.server.util.TrackingID;

public class ProjectMapper {

    public static Protocol.ProjectInfo map(Config.Configuration configuration, User user, Project project) {
        Protocol.ProjectInfo.Builder b = Protocol.ProjectInfo.newBuilder()
                .setId(project.getId())
                .setName(project.getName())
                .setDescription(project.getDescription())
                .setOwner(UserMapper.map(project.getOwner()))
                .setCreated(project.getCreated().getTime())
                .setLastUpdated(project.getLastUpdated().getTime())
                .setRepositoryUrl(String.format("http://%s:%d%s/%d", configuration.getHostname(),
                        configuration.getGitPort(),
                        ResourceUtil.getGitBaseUri(configuration),
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
            b.addMembers(UserMapper.map(u));
        }

        return b.build();
    }
}
