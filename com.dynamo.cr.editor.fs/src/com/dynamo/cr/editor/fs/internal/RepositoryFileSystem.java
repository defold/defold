package com.dynamo.cr.editor.fs.internal;

import java.net.URI;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.ws.rs.core.UriBuilder;

import org.apache.http.NameValuePair;
import org.apache.http.client.utils.URLEncodedUtils;
import org.eclipse.core.filesystem.EFS;
import org.eclipse.core.filesystem.IFileStore;
import org.eclipse.core.filesystem.provider.FileSystem;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceChangeListener;
import org.eclipse.core.resources.ResourcesPlugin;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.editor.fs.RepositoryFileSystemPlugin;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;

public class RepositoryFileSystem extends FileSystem implements IResourceChangeListener {

    private Map<String, CachingBranchClient> branchClients = new HashMap<String, CachingBranchClient>();
    private static Logger logger = LoggerFactory.getLogger(RepositoryFileSystem.class);

    public RepositoryFileSystem() {
        ClientConfig cc = new DefaultClientConfig();
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        ResourcesPlugin.getWorkspace().addResourceChangeListener(this, IResourceChangeEvent.PRE_REFRESH);
    }

    @Override
    protected void finalize() throws Throwable {
        super.finalize();
        // TODO: Better solution than in finalize()?
        ResourcesPlugin.getWorkspace().removeResourceChangeListener(this);
    }

    @Override
    public IFileStore getStore(URI uri) {
        if (RepositoryFileSystemPlugin.clientFactory == null) {
            // Factory not created yet
            return EFS.getNullFileSystem().getStore(uri);
        }

        String path = "/";

        List<NameValuePair> pairs = URLEncodedUtils.parse(uri, "UTF-8");
        for (NameValuePair p : pairs) {
            if (p.getName().equals("path")) {
                path = p.getValue();
            }
        }

        URI http_uri = UriBuilder.fromUri(uri).scheme("http").replaceQuery("").build();

        // The key is the URI with ?path= removed
        String key = UriBuilder.fromUri(uri).replaceQuery("").build().toString();
        IBranchClient branchClient;
        if (!branchClients.containsKey(key)) {
            IBranchClient bc;
            try {
                bc = RepositoryFileSystemPlugin.clientFactory.getBranchClient(http_uri);
            } catch (RepositoryException e) {
                // TODO: No access to StatusManager (in ui-package)
                // Better solution?
                logger.error(e.getMessage(), e);
                return EFS.getNullFileSystem().getStore(uri);
            }
            branchClients.put(key, new CachingBranchClient(bc));
        }
        branchClient = branchClients.get(key);

        return new RepositoryFileStore(branchClient, path);
    }

    @Override
    public void resourceChanged(IResourceChangeEvent event) {
        /*
         * TODO:
         * Somewhat temporary solution?
         * Workaround for actions on IBranchClient not created by this plugin, eg UpdateHandler()
         * We need to flush the cache.
         *
         * Should we merge client and fs such that we have a single factory with full control?
         */
        if (event.getType() == IResourceChangeEvent.PRE_REFRESH) {
            for (CachingBranchClient client : branchClients.values()) {
                client.flushAll();
            }
        }
    }
}
