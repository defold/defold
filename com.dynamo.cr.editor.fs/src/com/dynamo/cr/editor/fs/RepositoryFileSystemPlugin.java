package com.dynamo.cr.editor.fs;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.client.IClientFactory;

public class RepositoryFileSystemPlugin implements BundleActivator {

    public static final String PLUGIN_ID = "com.dynamo.cr.fs"; //$NON-NLS-1$
    static IClientFactory clientFactory = null;

    @Override
    public void start(BundleContext context) throws Exception {
    }

    @Override
    public void stop(BundleContext context) throws Exception {
    }

    public static void setClientFactory(IClientFactory clientFactory) {
        // NOTE: Somewhat budget with a static method here for configuration
        // but we don't currently have a better solution.
        RepositoryFileSystemPlugin.clientFactory = clientFactory;
    }

    public static void throwCoreExceptionError(String message, Throwable exception) throws CoreException {
        throw new CoreException(new Status(IStatus.ERROR, PLUGIN_ID, message, exception));
    }

}
