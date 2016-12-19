package com.dynamo.cr.server.resources.mappers;

import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.server.model.User;

public class UserMapper {

    public static Protocol.UserInfo map(User user) {
        return Protocol.UserInfo.newBuilder()
                .setId(user.getId())
                .setEmail(user.getEmail())
                .setFirstName(user.getFirstName())
                .setLastName(user.getLastName())
                .build();
    }
}
