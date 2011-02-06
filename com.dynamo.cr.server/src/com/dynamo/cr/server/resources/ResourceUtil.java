package com.dynamo.cr.server.resources;

import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;

public class ResourceUtil {
    public static UserInfo createUserInfo(User user) {
        UserInfo userInfo = UserInfo.newBuilder()
            .setId(user.getId())
            .setEmail(user.getEmail())
            .setFirstName(user.getFirstName())
            .setLastName(user.getLastName())
            .build();
        return userInfo;
    }

    public static ProjectInfo createProjectInfo(Project project) {
        ProjectInfo.Builder b = ProjectInfo.newBuilder()
            .setId(project.getId())
            .setName(project.getName())
            .setDescription(project.getDescription())
            .setOwner(createUserInfo(project.getOwner()))
            .setCreated(project.getCreated().getTime())
            .setLastUpdated(project.getLastUpdated().getTime());

        for (User user : project.getMembers()) {
            b.addMembers(createUserInfo(user));
        }

        return b.build();
    }

}
