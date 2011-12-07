package com.dynamo.cr.integrationtest;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import java.io.InputStream;
import java.net.URL;
import java.util.Enumeration;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IProjectDescription;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.junit.Before;
import org.osgi.framework.Bundle;

import com.dynamo.cr.editor.core.ILogger;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.ui.LoaderContext;

public abstract class AbstractNodeTest {

    IProject project;
    NullProgressMonitor monitor;
    protected INodeTypeRegistry nodeTypeRegistry;
    protected ILoaderContext loaderContext;

    public AbstractNodeTest() {
        super();
    }

    @Before
    public void setUp() throws Exception {
        project = ResourcesPlugin.getWorkspace().getRoot().getProject("test");
        monitor = new NullProgressMonitor();
        if (project.exists()) {
            project.delete(true, monitor);
        }
        project.create(monitor);
        project.open(monitor);

        IProjectDescription pd = project.getDescription();
        pd.setNatureIds(new String[] { "com.dynamo.cr.editor.core.crnature" });
        project.setDescription(pd, monitor);

        Bundle bundle = Platform.getBundle("com.dynamo.cr.integrationtest");
        Enumeration<URL> entries = bundle.findEntries("/test", "*", true);
        while (entries.hasMoreElements()) {
            URL url = entries.nextElement();
            IPath path = new Path(url.getPath()).removeFirstSegments(1);
            // Create path of url-path and remove first element, ie /test/sounds/ -> /sounds
            if (url.getFile().endsWith("/")) {
                project.getFolder(path).create(true, true, monitor);
            } else {
                InputStream is = url.openStream();
                IFile file = project.getFile(path);
                file.create(is, true, monitor);
                is.close();
            }
        }
        this.nodeTypeRegistry = Activator.getDefault().getNodeTypeRegistry();
        this.loaderContext = new LoaderContext(project, nodeTypeRegistry, mock(ILogger.class));
    }

    private boolean containsMessage(IStatus status, String message) {
        if (status.isMultiStatus()) {
            for (IStatus childStatus : status.getChildren()) {
                if (containsMessage(childStatus, message))
                    return true;
            }
            return false;
        } else {
            return status.getMessage().equals(message);
        }
    }

    protected void assertMessageExists(IStatus status, String message) {
        if (!containsMessage(status, message)) {
            assertTrue(String.format("Message %s not found in %s", message, status.toString()), false);
        }
    }

}
