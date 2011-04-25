package com.dynamo.cr.client;

import com.dynamo.cr.protocol.proto.Protocol.UserInfo;

public interface IUsersClient extends IClient {

    public UserInfo getUserInfo(String userName) throws RepositoryException;

}
