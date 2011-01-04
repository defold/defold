package com.dynamo.cr.editor.fs;

import java.net.URI;
import java.util.List;

import javax.ws.rs.core.UriBuilder;

import org.apache.http.NameValuePair;
import org.apache.http.client.utils.URLEncodedUtils;
import org.eclipse.core.filesystem.IFileStore;
import org.eclipse.core.filesystem.provider.FileSystem;

import com.dynamo.cr.client.BranchClient;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.editor.Activator;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;

public class RepositoryFileSystem extends FileSystem {

    public RepositoryFileSystem() {
        ClientConfig cc = new DefaultClientConfig();
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);
    }

    @Override
    public IFileStore getStore(URI uri) {

        String path = "/";

        List<NameValuePair> pairs = URLEncodedUtils.parse(uri, "UTF-8");
        for (NameValuePair p : pairs) {
            if (p.getName().equals("path")) {
                path = p.getValue();
            }
        }

        URI http_uri = UriBuilder.fromUri(uri).scheme("http").replaceQuery("").build();
        BranchClient branch_client = Activator.getDefault().getClientFactory().getBranchClient(http_uri);

        return new RepositoryFileStore(branch_client, path);
    }
}
