package com.dynamo.cr.client;

import com.dynamo.cr.protocol.proto.Protocol.UserInfo;

public interface IUsersClient {

    public UserInfo getUserInfo(String userName) throws RepositoryException;

}
