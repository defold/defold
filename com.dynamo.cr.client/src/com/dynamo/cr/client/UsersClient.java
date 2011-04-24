package com.dynamo.cr.client;

import java.net.URI;

import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.sun.jersey.api.client.Client;

public class UsersClient extends BaseClient implements IUsersClient {

    public UsersClient(IClientFactory factory, URI uri, Client client) {
        super(factory);
        this.client = client;
        resource = client.resource(uri);
    }

    @Override
    public UserInfo getUserInfo(String userName) throws RepositoryException {
        return wrapGet(String.format("/%s", userName), UserInfo.class);
    }

}
