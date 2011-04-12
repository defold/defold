package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceChangeListener;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;

import com.dynamo.cr.scene.graph.CreateException;

public class ResourceFactory implements IResourceFactory, IResourceChangeListener {

    private IContainer contentRoot;
    private Map<IPath, Resource> resources = new HashMap<IPath, Resource>();

    Map<String, IResourceLoader> loaders = new HashMap<String, IResourceLoader>();

    public boolean canLoad(String path) {
        String extension = path.substring(path.lastIndexOf('.') + 1);
        return loaders.containsKey(extension);
    }

    public void addLoader(String extension, IResourceLoader loader) {
        loaders.put(extension, loader);
    }

    public ResourceFactory(IContainer contentRoot) {
        this.contentRoot = contentRoot;
    }

    protected IFile getFile(String name) throws IOException, CoreException {
        IFile f;
        if (name.startsWith("/")) {
            f = contentRoot.getWorkspace().getRoot().getFile(new Path(name));
        }
        else {
            f = contentRoot.getFile(new Path(name));
        }
        return f;
    }

    @Override
    public Resource load(IProgressMonitor monitor, String path)
            throws IOException, CreateException, CoreException {
        IFile file = getFile(path);
        return load(monitor, path, file.getContents());
    }

    @Override
    public Resource load(IProgressMonitor monitor, String path, InputStream in)
            throws IOException, CreateException, CoreException {

        // Normalize path by using the path-class
        IFile file = getFile(path);
        IPath projectPath = file.getProjectRelativePath();
        if (!resources.containsKey(projectPath)) {
            IResourceLoader loader = loaders.get(file.getFileExtension());
            if (loader == null)
                throw new CreateException("No support for loading " + path);

            InputStream stream = file.getContents();
            // avoid inifinite recursion
            resources.put(projectPath, null);
            Resource resource = loader.load(monitor, path, stream, this);
            resources.put(projectPath, resource);
            return resource;
        }

        return resources.get(projectPath);
    }

    @Override
    public void resourceChanged(IResourceChangeEvent event) {

        try {
            final IResourceFactory factory = this;

            if (event.getType() == IResourceChangeEvent.POST_CHANGE) {
                event.getDelta().accept(new IResourceDeltaVisitor() {

                    @Override
                    public boolean visit(IResourceDelta delta) {
                        if ((delta.getKind() & IResourceDelta.CHANGED) == IResourceDelta.CHANGED) {
                            IPath path = delta.getResource().getProjectRelativePath();
                            Resource resource = resources.get(path);
                            if (resource != null) {
                                IResourceLoader loader = loaders.get(path.getFileExtension());
                                String name = path.toPortableString();
                                NullProgressMonitor monitor = new NullProgressMonitor();
                                try {
                                    loader.reload(resource, monitor, name, contentRoot.getProject().getFile(path).getContents(), factory);
                                    resource.fireChanged();
                                } catch (CreateException e) {
                                    e.printStackTrace();
                                } catch (IOException e) {
                                    e.printStackTrace();
                                } catch (CoreException e) {
                                    e.printStackTrace();
                                }
                                return false;
                            }
                        }
                        return true;
                    }
                });
            }
        } catch (CoreException e) {
            e.printStackTrace();
        }
    }
}
